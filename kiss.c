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

//	Version 409p March 2005 Allow Multidigit COM Ports

//  Version 410h Jan 2009 Changes for Win98 Virtual COM
//		Open \\.\com instead of //./COM
//		Extra Dignostics

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE

#ifdef MACBPQ
#define NOI2C
#endif


#ifndef WIN32

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <syslog.h>


//#include <netax25/ttyutils.h>
//#include <netax25/daemon.h>

#ifdef NOI2C
int i2c_smbus_write_byte()
{
	return -1;
}

int i2c_smbus_read_byte()
{
	return -1;
}
#else
#include "i2c-dev.h"
#endif

//#define I2C_TIMEOUT	0x0702	/* set timeout - call with int 		*/

/* this is for i2c-dev.c	*/
//#define I2C_SLAVE	0x0703	/* Change slave address			*/
				/* Attn.: Slave address is 7 or 10 bits */

#endif


#include "CHeaders.h"
#include "kiss.h"



#define FEND 0xC0 
#define FESC 0xDB
#define TFEND 0xDC
#define TFESC 0xDD

#define STX	2			// NETROM CONTROL CODES
#define ETX	3
#define DLE	0x10

#define CHECKSUM 1
#define POLLINGKISS	2			// KISSFLAGS BITS
#define ACKMODE	4				// CAN USE ACK REQURED FRAMES
#define POLLEDKISS	8			// OTHER END IS POLLING US
#define D700 16					// D700 Mode (Escape "C" chars
#define TNCX 32					// TNC-X Mode (Checksum of ACKMODE frames includes ACK bytes
#define PITNC 64				// PITNC Mode - can reset TNC with FEND 15 2
#define NOPARAMS 128			// Don't send SETPARAMS frame
#define FLDIGI 256				// Support FLDIGI COmmand Frames
#define TRACKER 512				// SCS Tracker. Need to set KISS Mode 

int WritetoConsoleLocal(char * buff);
VOID INITCOMMON(struct KISSINFO * PORT);
struct PORTCONTROL * CHECKIOADDR(struct PORTCONTROL * OURPORT);
VOID INITCOM(struct KISSINFO * PORTVEC);
VOID SENDFRAME(struct KISSINFO * KISS, UINT * Buffer);
int ConnecttoUZ7HOTCP(NPASYINFO ASY);
int KISSGetTCPMessage(NPASYINFO ASY);

extern struct PORTCONTROL * PORTTABLE;
extern int	NUMBEROFPORTS;
extern UINT TRACE_Q;

#define TICKS 10	// Ticks per sec


// temp for testing

char lastblock[500];
int lastcount;

UCHAR ENCBUFF[600];

int ASYSEND(struct PORTCONTROL * PortVector, char * buffer, int count)
{
	NPASYINFO Port = KISSInfo[PortVector->PORTNUMBER];

	if (Port == NULL)
		return 0;

	if (PortVector->PORTTYPE == 22)			// i2c
	{
#ifndef WIN32
		int i = count;
		UCHAR * ptr = buffer;
		int ret;
		struct KISSINFO * KISS = (struct KISSINFO *)PortVector;

		memcpy(lastblock, buffer, count);
		lastcount = count;

		KISS->TXACTIVE = TRUE;
		
		while (i--)
		{
			ret = i2c_smbus_write_byte(Port->idComDev, *(ptr));
			if (ret == -1)
			{
				Debugprintf ("i2c Write Error\r");
				usleep(1000);
				ret = i2c_smbus_write_byte(Port->idComDev, *(ptr));
			}		
			ptr++;
		}

#endif
		return 0;
	}
	else if (PortVector->PORTIPADDR.s_addr || PortVector->KISSSLAVE)		// KISS over UDP/TCP
		if (PortVector->KISSTCP)
			send(Port->sock, buffer, count, 0);
		else
			sendto(Port->sock, buffer, count, 0, (struct sockaddr *)&Port->destaddr, sizeof(Port->destaddr));
	else
		WriteCommBlock(Port, buffer, count);
	
	return 0;
}

VOID EnableFLDIGIReports(struct PORTCONTROL * PORT)
{
	struct KISSINFO * KISS = (struct KISSINFO *)PORT;
	UCHAR Buffer[256];
	UCHAR * ptr = Buffer;;

	*(ptr++) = FEND;
	*(ptr++) = KISS->OURCTRL | 6;
	ptr += sprintf(ptr, "%s", "TNC: MODEM: RSIDBCAST:ON TRXSBCAST:ON TXBEBCAST:ON");
//	ptr += sprintf(ptr, "%s", "TNC");
	*(ptr++) = FEND;
	
	ASYSEND(PORT, Buffer, ptr - &Buffer[0]);
}


VOID ASYDISP(struct PORTCONTROL * PortVector)
{
	char Msg[80];

	if (PortVector->PORTIPADDR.s_addr)

		// KISS over UDP

		if (PortVector->KISSTCP)
			sprintf(Msg,"TCPKISS IP %s Port %d Chan %c \n",
				inet_ntoa(PortVector->PORTIPADDR), PortVector->IOBASE, PortVector->CHANNELNUM);
		else
			sprintf(Msg,"UDPKISS IP %s Port %d/%d Chan %c \n",
				inet_ntoa(PortVector->PORTIPADDR), PortVector->ListenPort, PortVector->IOBASE, PortVector->CHANNELNUM);
		



	else
		if (PortVector->SerialPortName)
			sprintf(Msg,"ASYNC %s Chan %c \n", PortVector->SerialPortName, PortVector->CHANNELNUM);
		else
			sprintf(Msg,"ASYNC COM%d Chan %c \n", PortVector->IOBASE, PortVector->CHANNELNUM);
		
	WritetoConsoleLocal(Msg);
	return;
}


int	ASYINIT(int comport, int speed, struct PORTCONTROL * PortVector, char Channel )
{
	char Msg[80];
	NPASYINFO npKISSINFO;
	int BPQPort = PortVector->PORTNUMBER;

	if (PortVector->PORTTYPE == 22)			// i2c
	{
#ifdef WIN32

		sprintf(Msg,"I2C is not supported on WIN32 systems\n");
		WritetoConsoleLocal(Msg);

		return 0;
#else
#ifdef NOI2C

		sprintf(Msg,"I2C is not supported on this systems\n");
		WritetoConsoleLocal(Msg);

		return 0;
#else
		char i2cname[30];
		int fd;
		int retval;

		PortVector->KISSFLAGS |= CHECKSUM |TNCX;		// i2c TNCs need chgecksum and TNCX Mode

		sprintf(Msg,"I2C Bus %d Addr %d Chan %c ", PortVector->INTLEVEL, comport, Channel);
		WritetoConsoleLocal(Msg);

		npKISSINFO = KISSInfo[PortVector->PORTNUMBER] = CreateKISSINFO(comport, speed);

		if (NULL == npKISSINFO)
			return ( FALSE ) ;

		npKISSINFO->RXBCOUNT=0;
		npKISSINFO->MSGREADY=FALSE;
		npKISSINFO->RXBPTR=&npKISSINFO->RXBUFFER[0]; 
		npKISSINFO->RXMPTR=&npKISSINFO->RXMSG[0];

		// Open and configure the i2c interface
		
		sprintf(i2cname, "/dev/i2c-%d", PortVector->INTLEVEL);
                         
		fd = open(i2cname, O_RDWR);
		if (fd < 0)
		{
			printf("Cannot find i2c bus %s\n", i2cname);
		}
		
		//	check_funcs(file, size, daddress, pec)
	  
	 	retval = ioctl(fd,  I2C_SLAVE, comport);
		
		if(retval == -1)
		{
			printf("Cannot open i2c device %x\n", comport);
		}
 
 		ioctl(fd,  I2C_TIMEOUT, 100);

		npKISSINFO->idComDev = fd;

		// Reset the TNC and wait for completion
	
		i2c_smbus_write_byte(fd, FEND);		
		i2c_smbus_write_byte(fd, 15);
		i2c_smbus_write_byte(fd, 2);
	
		sleep(2);

#endif
#endif

	}
	else if (PortVector->PORTIPADDR.s_addr || PortVector->KISSSLAVE)
	{
		SOCKET sock;
		u_long param=1;
		BOOL bcopt=TRUE;
		struct sockaddr_in sinx;

		// KISS over UDP or TCP

		if (PortVector->ListenPort == 0)
			PortVector->ListenPort = PortVector->IOBASE;

		if (PortVector->KISSTCP)
			sprintf(Msg,"TCPKISS IP %s Port %d Chan %c ",
				inet_ntoa(PortVector->PORTIPADDR), PortVector->IOBASE, Channel);
		else
			sprintf(Msg,"UDPKISS IP %s Port %d/%d Chan %c ",
				inet_ntoa(PortVector->PORTIPADDR), PortVector->ListenPort, PortVector->IOBASE, Channel);
		
		WritetoConsoleLocal(Msg);
		
		npKISSINFO = (NPASYINFO) zalloc(sizeof(ASYINFO));

		memset(npKISSINFO, 0, sizeof(NPASYINFO));
		npKISSINFO->bPort = comport;
  
		KISSInfo[PortVector->PORTNUMBER] = npKISSINFO;

		npKISSINFO->RXBCOUNT=0;
		npKISSINFO->MSGREADY=FALSE;
		npKISSINFO->RXBPTR=&npKISSINFO->RXBUFFER[0]; 
		npKISSINFO->RXMPTR=&npKISSINFO->RXMSG[0];

		npKISSINFO->destaddr.sin_family = AF_INET;
		npKISSINFO->destaddr.sin_addr.s_addr = PortVector->PORTIPADDR.s_addr;		
		npKISSINFO->destaddr.sin_port = htons(PortVector->IOBASE);

		if (PortVector->KISSTCP)
		{
			if (PortVector->KISSSLAVE)
			{
				// Bind and Listen

				npKISSINFO->sock = sock = socket(AF_INET,SOCK_STREAM,0);
				ioctl(sock, FIONBIO, &param);

				sinx.sin_family = AF_INET;
				sinx.sin_addr.s_addr = INADDR_ANY;		
				sinx.sin_port = htons(PortVector->ListenPort);

				if (bind(sock, (struct sockaddr *) &sinx, sizeof(sinx)) != 0 )
				{
					//	Bind Failed

					int err = WSAGetLastError();
					Consoleprintf("Bind Failed for KISS TCP port %d - error code = %d", PortVector->ListenPort, err);
					closesocket(sock);
				}
				else
				{
					if (listen(sock, 1) < 0)
					{
						int err = WSAGetLastError();
						Consoleprintf("Listen Failed for KISS TCP port %d - error code = %d", PortVector->ListenPort, err);
						closesocket(sock);
					}
					else
						npKISSINFO->Listening = TRUE;	
				}
			}
			else
				ConnecttoUZ7HOTCP(npKISSINFO);
		}
		else
		{
			npKISSINFO->sock = sock = socket(AF_INET,SOCK_DGRAM,0);
			ioctl(sock, FIONBIO, &param);

			setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char FAR *)&bcopt,4);

			sinx.sin_family = AF_INET;
			sinx.sin_addr.s_addr = INADDR_ANY;		
			sinx.sin_port = htons(PortVector->ListenPort);

			if (bind(sock, (struct sockaddr *) &sinx, sizeof(sinx)) != 0 )
			{
				//	Bind Failed

				int err = WSAGetLastError();
				Consoleprintf("Bind Failed for UDP port %d - error code = %d", PortVector->ListenPort, err);
			}
		}
	}

	else
	{
		if (PortVector->SerialPortName)
			sprintf(Msg,"ASYNC %s Chan %c ", PortVector->SerialPortName, Channel);
		else
			sprintf(Msg,"ASYNC COM%d Chan %c ", comport, Channel);

		WritetoConsoleLocal(Msg);

		npKISSINFO = KISSInfo[PortVector->PORTNUMBER] = CreateKISSINFO(comport, speed);

		if (NULL == npKISSINFO)
			return ( FALSE ) ;

		npKISSINFO->RXBCOUNT=0;
		npKISSINFO->MSGREADY=FALSE;
		npKISSINFO->RXBPTR=&npKISSINFO->RXBUFFER[0]; 
		npKISSINFO->RXMPTR=&npKISSINFO->RXMSG[0];

		OpenConnection(PortVector, comport);

	}

	npKISSINFO->Portvector = PortVector; 

	WritetoConsoleLocal("\n");

	return (0);
}

NPASYINFO CreateKISSINFO( int port,int speed )
{
   NPASYINFO   npKISSINFO ;

   if (NULL == (npKISSINFO = (NPASYINFO) zalloc(sizeof(ASYINFO))))
      return (NPASYINFO)0;

   // initialize TTY info structure

	npKISSINFO->idComDev = 0 ;
	npKISSINFO->bPort = port;
	npKISSINFO->dwBaudRate = speed;
	
	return (npKISSINFO);
}



HANDLE OpenConnection(struct PORTCONTROL * PortVector, int port)
{
	NPASYINFO npKISSINFO = KISSInfo[PortVector->PORTNUMBER];
	HANDLE  ComDev ;


	if (npKISSINFO == NULL)
		return 0;

	if (PortVector->SerialPortName)
		ComDev = OpenCOMPort(PortVector->SerialPortName, npKISSINFO->dwBaudRate, TRUE, TRUE, FALSE, 0);
	else
		ComDev = OpenCOMPort((VOID *)port, npKISSINFO->dwBaudRate, TRUE, TRUE, FALSE, 0);
	
	if (ComDev)
		npKISSINFO->idComDev = ComDev;

	if (PortVector->KISSFLAGS & PITNC)
	{
		// RFM22/23 module  or TNC-PI- send a reset

		struct KISSINFO * KISS = (struct KISSINFO *) PortVector;

		ENCBUFF[0] = FEND;
		ENCBUFF[1] = KISS->OURCTRL | 15;	// Action command
		ENCBUFF[2] = 2;						// Reset command

		ASYSEND(PortVector, ENCBUFF, 3);
	}

	if (PortVector->KISSFLAGS & TRACKER)
	{
		// SCS Tracker - Send Enter KISS (CAN)(ESC)@K(CR)

		struct KISSINFO * KISS = (struct KISSINFO *) PortVector;

		memcpy(ENCBUFF, "\x18\x1b@K\r", 5);	// Enter KISS

		ASYSEND(PortVector, ENCBUFF, 5);
	}

	return ComDev;
}
int ReadCommBlock(NPASYINFO npKISSINFO, char * lpszBlock, int nMaxLength )
{
	if (npKISSINFO->idComDev == 0)
		return 0;

	return ReadCOMBlock(npKISSINFO->idComDev, lpszBlock, nMaxLength);
}

static BOOL WriteCommBlock(NPASYINFO npKISSINFO, char * lpByte, DWORD dwBytesToWrite)
{
	if (npKISSINFO->idComDev == 0)
		return 0;

	return WriteCOMBlock(npKISSINFO->idComDev, lpByte, dwBytesToWrite);
}

VOID KISSCLOSE(struct PORTCONTROL * PortVector)
{
	NPASYINFO Port = KISSInfo[PortVector->PORTNUMBER];

	if (Port == NULL)
		return;
	
	if (PortVector->PORTIPADDR.s_addr)
		closesocket(Port->sock);
	else
		CloseCOMPort(Port->idComDev);
 
	free(Port);
	KISSInfo[PortVector->PORTNUMBER] = NULL;

	return;
}
VOID CloseKISSPort(struct PORTCONTROL * PortVector)
{
	// Just close he device - leave reast of info intact

	NPASYINFO Port = KISSInfo[PortVector->PORTNUMBER];

	if (Port == NULL)
		return;
	
	if (PortVector->PORTIPADDR.s_addr == 0)
	{
		CloseCOMPort(Port->idComDev);
		Port->idComDev = 0;
	}
}

static void CheckReceivedData(struct PORTCONTROL * PORT, NPASYINFO npKISSINFO)
{
 	UCHAR c;
	int nLength = 0;

	if (npKISSINFO->RXBCOUNT == 0)
	{	
		//
		//	Check com buffer
		//

		if (PORT->PORTTYPE == 22)			// i2c
		{
#ifndef WIN32
			nLength = i2cPoll(PORT, npKISSINFO);
#else
			nLength = 0;
#endif
		}
		else if (PORT->PORTIPADDR.s_addr || PORT->KISSSLAVE)		// KISS over UDP
		{
			if (PORT->KISSTCP)
			{
				nLength = KISSGetTCPMessage(npKISSINFO);
			}
			else
			{
				struct sockaddr_in rxaddr;
				int addrlen = sizeof(struct sockaddr_in);

				nLength = recvfrom(npKISSINFO->sock, &npKISSINFO->RXBUFFER[0], MAXBLOCK - 1, 0, (struct sockaddr *)&rxaddr, &addrlen);
	
				if (nLength < 0)
				{
					int err = WSAGetLastError();
		//			if (err != 11)
		//				printf("KISS Error %d %d\n", nLength, err);
					nLength = 0;
				}
			}
		}
		else
			nLength = ReadCommBlock(npKISSINFO, (char *) &npKISSINFO->RXBUFFER, MAXBLOCK - 1);;
	
		npKISSINFO->RXBCOUNT = nLength;
		npKISSINFO->RXBPTR = (UCHAR *)&npKISSINFO->RXBUFFER; 
	}

	if (npKISSINFO->RXBCOUNT == 0)
		return;

	while (npKISSINFO->RXBCOUNT != 0)
	{
		npKISSINFO->RXBCOUNT--;

		c = *(npKISSINFO->RXBPTR++);
	
		if (npKISSINFO->ESCFLAG)
		{
			//
			//	FESC received - next should be TFESC or TFEND

			npKISSINFO->ESCFLAG = FALSE;

			if (c == TFESC)
				c=FESC;
	
			if (c == TFEND)
				c=FEND;

		}
		else
		{
			switch (c)
			{
			case FEND:		
	
				//
				//	Either start of message or message complete
				//
				
				if (npKISSINFO->RXMPTR == (UCHAR *)&npKISSINFO->RXMSG)
				{
					struct KISSINFO * KISS = (struct KISSINFO *)PORT;

					// Start of Message. If polling, extend timeout

					if (PORT->KISSFLAGS & POLLINGKISS)
						KISS->POLLFLAG = 5*TICKS;		// 5 SECS - SHOULD BE PLENTY

					continue;
				}

				npKISSINFO->MSGREADY = TRUE;
				return;

			case FESC:
		
				npKISSINFO->ESCFLAG = TRUE;
				continue;

			}
		}
		
		//
		//	Ok, a normal char
		//

		*(npKISSINFO->RXMPTR++) = c;

		// if control byte, and equal to 0x0e, must set ready - poll responses dont have a trailing fend

		if (((c & 0x0f) == 0x0e) && npKISSINFO->RXMPTR - (UCHAR *)&npKISSINFO->RXMSG == 1)
		{
			npKISSINFO->MSGREADY = TRUE;
			return;
		}
	}

	if (npKISSINFO->RXMPTR - (UCHAR *)&npKISSINFO->RXMSG > 500)
		npKISSINFO->RXMPTR = (UCHAR *)&npKISSINFO->RXMSG;
	
 	return;
}

// Code moved from KISSASM
	
unsigned short CRCTAB[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd, 
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 
0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 
0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 
0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 
0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 
0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a, 
0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 
0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 
0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 
0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 
0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70, 
0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 
0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 
0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 
0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 
0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 
0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 
0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 
0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c, 
0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 
0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 
0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 
0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 
0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 
0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 
0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 
0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78 
}; 



VOID KISSINIT(struct KISSINFO * KISS)
{
	PORTCONTROLX * PORT = (struct PORTCONTROL *)KISS;
	struct KISSINFO * FIRSTCHAN = NULL;

	PORT->PORTINTERLOCK = 0;	// CANT USE INTERLOCK ON KISS

	if (PORT->CHANNELNUM == 0)
		PORT->CHANNELNUM = 'A';

	FIRSTCHAN = (struct KISSINFO *)CHECKIOADDR(PORT);	// IF ANOTHER ENTRY FOR THIS ADDR
								// MAY BE CHANGED IN ANOTHER CHANNEL USED

	KISS->OURCTRL = (PORT->CHANNELNUM - 'A') << 4;	// KISS CONTROL

	Debugprintf("FirstChan %X", FIRSTCHAN);

	if (FIRSTCHAN)
	{
		//	THIS IS NOT THE FIRST PORT ON THIS I/O ADDR - WE MUST BE USING
		//	AN ADDRESSABLE PROTOCOL - IE KISS AS DEFINED FOR KPC4 ETC

 		KISS->FIRSTPORT = FIRSTCHAN;		// QUEUE TX FRAMES ON FIRST
	
		//	SET UP SUBCHANNEL CHAIN - ALL PORTS FOR THIS IO ADDR ARE CHAINED

		while (FIRSTCHAN->SUBCHAIN)
		{
			FIRSTCHAN = FIRSTCHAN->SUBCHAIN;
		}

		FIRSTCHAN->SUBCHAIN = KISS;		// PUT OURS ON END
		INITCOMMON(KISS);
		ASYDISP(PORT);
	}
	else
		INITCOM(KISS);
			
	//	Display to Console
	
	
}

VOID INITCOM(struct KISSINFO * KISS)
{
	struct PORTCONTROL * PORT = (struct PORTCONTROL *)KISS;

	//	FIRST PORT USING THIS IO ADDRESS

	KISS->FIRSTPORT = KISS;
	KISS->POLLPOINTER = KISS;	// SET FIRST PORT TO POLL

	INITCOMMON(KISS);			// SET UP THE PORT

	//	ATTACH WIN32 ASYNC DRIVER

	ASYINIT(PORT->IOBASE, PORT->BAUDRATE, PORT, PORT->CHANNELNUM);

	if (PORT->KISSFLAGS & FLDIGI)
		EnableFLDIGIReports(PORT);

	return;
}


//struct PORTCONTROL * CHECKIOADDR(struct PORTCONTROL * OURPORT)
struct PORTCONTROL * CHECKIOADDR(struct PORTCONTROL * OURPORT)
{
	//	SEE IF ANOTHER PORT IS ALREADY DEFINED ON THIS CARD

	struct PORTCONTROL * PORT = PORTTABLE;
	int n = NUMBEROFPORTS;

	while (n--)
	{
		if (PORT == OURPORT)		// NONE BEFORE OURS
			return NULL;		// None before us
	
		if (PORT->PORTTYPE > 12)		// INTERNAL or EXTERNAL?
		{
			PORT = PORT->PORTPOINTER;	// YES, SO IGNORE
			continue;
		}

		if (OURPORT->SerialPortName)
		{
			// We are using a name
			
			if (PORT->SerialPortName && strcmp(PORT->SerialPortName, OURPORT->SerialPortName) == 0)
				return PORT;
		}
		else
		{
			// Using numbers not names

			if (PORT->IOBASE == OURPORT->IOBASE)
				return PORT;			// ANOTHER FOR SAME ADDRESS
		}

		PORT = PORT->PORTPOINTER;	
	}

	return NULL;
}

VOID INITCOMMON(struct KISSINFO * KISS)
{
	struct PORTCONTROL * PORT = (struct PORTCONTROL *)KISS;

	if (PORT->PROTOCOL == 2)	// NETROM?
	{
		PORT->KISSFLAGS = 0;	//	CLEAR KISS OPTIONS, JUST IN CASE!

//	CMP	FULLDUPLEX[EBX],1	; NETROM DROPS RTS TO INHIBIT!	
//	JE SHORT NEEDRTS
//
//	MOV	AL,9			; OUT2 DTR
	}

//	IF KISS, SET TIMER TO SEND KISS PARAMS

	if (PORT->PROTOCOL != 2)	// NETROM?
		if ((PORT->KISSFLAGS & NOPARAMS) == 0)
			PORT->PARAMTIMER = TICKS*30	; //30 SECS FOR TESTING

}

VOID KISSTX(struct KISSINFO * KISS, UINT * Buffer)
{
	struct PORTCONTROL * PORT = (struct PORTCONTROL *)KISS;

	//	START TRANSMISSION

	KISS = KISS->FIRSTPORT;		// ALL FRAMES GO ON SAME Q

	if ((PORT->KISSFLAGS & POLLEDKISS) || KISS->KISSTX_Q || KISS->FIRSTPORT->POLLFLAG || KISS->TXACTIVE)
	{
		// POLLED or ALREADY SOMETHING QUEUED or POLL OUTSTANDING - MUST QUEUE

		C_Q_ADD(&KISS->KISSTX_Q, Buffer);
		return;
	}

/*
;	IF NETROM PROTOCOL AND NOT FULL DUPLEX AND BUSY, QUEUE IT
;
	CMP	PROTOCOL[EBX],2		; NETROM?
	JNE SHORT DONTCHECKDCD		; NO

	CMP	FULLDUPLEX[EBX],1
	JE SHORT DONTCHECKDCD		; FULLDUP - NO CHECK
;
;	NETROM USES RTS, CROSS-CONNECTED TO CTS AT OTHER END, TO MEAN
;	NOT BUSY
;
;	MOV	DX,MSR[EBX]
;	IN	AL,DX
;
;	TEST	AL,CTSBIT		; CTS HIGH?
;	JZ SHORT QUEUEIT			; NO, SO QUEUE FRAME
;
;	GOING TO SEND - DROP RTS TO INTERLOCK OTHERS 
;
;	MOV	DX,MCR[EBX]
;	MOV	AL,09H			; DTR OUT2
;
;	OUT	DX,AL
;
;	MAKE SURE CTS IS STILL ACTIVE - IF NOT, WE HAVE A COLLISION,
;	SO RELEASE RTS AND WAIT
;
;	DELAY

;	MOV	DX,MSR[EBX]
;	IN	AL,DX
;	TEST	AL,CTSBIT
;	JNZ SHORT DONTCHECKDCD		; STILL HIGH, SO CAN SEND
;
;	RAISE RTS AGAIN, AND QUEUE FRAME
;
;	DELAY

;	MOV	DX,MCR[EBX]
;	MOV	AL,0BH			; RTS DTR OUT2
;
;	OUT	DX,AL
;
	JMP	QUEUEIT

DONTCHECKDCD:
*/

	SENDFRAME(KISS, Buffer);
}

VOID SENDFRAME(struct KISSINFO * KISS, UINT * Buffer)
{
	PPORTCONTROL PORT = (struct PORTCONTROL *)KISS;
	struct _MESSAGE * Message = (struct _MESSAGE *)Buffer;
	UCHAR c;

	int Portno;
	char * ptr1, * ptr2;
	int Len;

	//	GET REAL PORT TABLE ENTRY - IF MULTIPORT, FRAME IS QUEUED ON FIRST

	if (PORT->PROTOCOL == 2)			// NETROM
	{
		return;
	}
	
	Portno = Message->PORT;
	
	while (KISS->PORT.PORTNUMBER != Portno)
	{
		KISS = KISS->SUBCHAIN;

		if (KISS == NULL)
		{
			ReleaseBuffer(Buffer);
			return;
		}
	}

	//	Encode frame

	ptr1 = &Message->DEST[0];
	Len = Message->LENGTH - 7;
	ENCBUFF[0] = FEND;
	ENCBUFF[1] = KISS->OURCTRL;
	ptr2 = &ENCBUFF[2];

	KISS->TXCCC = 0;

	//	See if ACKMODE needed
	
	if (PORT->KISSFLAGS & ACKMODE)
	{
		UINT ACKWORD = Buffer[(BUFFLEN-4)/4];

		if (ACKWORD)					// Frame Needs ACK
		{
			ENCBUFF[1] |= 0x0c;			// ACK OPCODE 
			ACKWORD -= (UINT)LINKS;		// Con only send 16 bits, so use offset into LINKS
			ENCBUFF[2] = ACKWORD & 0xff;
			ENCBUFF[3] = (ACKWORD >> 8) &0xff;

			// have to reset flag so trace doesnt clear it

			Buffer[(BUFFLEN-4)/4] = 0;

			if (PORT->KISSFLAGS & TNCX)
			{
				// Include ACK bytes in Checksum

				KISS->TXCCC ^= ENCBUFF[2];
				KISS->TXCCC ^= ENCBUFF[3];
			}
			ptr2 = & ENCBUFF[4];
		}
	}

	KISS->TXCCC ^= ENCBUFF[1];

	while (Len--)
	{
		c = *(ptr1++);
		KISS->TXCCC  ^= c;

		switch (c)
		{
		case FEND:
			(*ptr2++) = FESC;
			(*ptr2++) = TFEND;
			break;

		case FESC:

			(*ptr2++) = FESC;
			(*ptr2++) = TFESC;
			break;

		case 'C':
			
			if (PORT->KISSFLAGS & D700)
			{
				(*ptr2++) = FESC;
				(*ptr2++) = 'C';
				break;
			}

			// Drop through

		default:

			(*ptr2++) = c;
		}
	}

	// If using checksum, send it

	if (PORT->KISSFLAGS & CHECKSUM)
	{
		c = (UCHAR)KISS->TXCCC;

		// On TNC-X based boards, it is difficult to cope with an encoded CRC, so if
		// CRC is FEND, send it as 0xc1. This means we have to accept 00 or 01 as valid.
		// which is a slight loss in robustness

		if (c == FEND && (PORT->KISSFLAGS & TNCX))
		{	
			(*ptr2++) = FEND + 1;
		}
		else
		{
			switch (c)
			{
			case FEND:
				(*ptr2++) = FESC;
				(*ptr2++) = TFEND;
				break;

			case FESC:
				(*ptr2++) = FESC;
				(*ptr2++) = TFESC;
				break;

			default:
				(*ptr2++) = c;
			}
		}
	}

	(*ptr2++) = FEND;

	ASYSEND(PORT, ENCBUFF, ptr2 - (char *)ENCBUFF);

	// Pass buffer to trace routines

	C_Q_ADD(&TRACE_Q, Buffer);
}


VOID KISSTIMER(struct KISSINFO * KISS)
{
	struct PORTCONTROL * PORT = (struct PORTCONTROL *)KISS;
	UINT * Buffer;

	//	SEE IF TIME TO REFRESH KISS PARAMS

	if (((PORT->KISSFLAGS & (POLLEDKISS | NOPARAMS)) == 0) && PORT->PROTOCOL != 2)
	{
		PORT->PARAMTIMER--;
		
		if (PORT->PARAMTIMER == 0)
		{
			//	QUEUE A 'SET PARAMS' FRAME
	
			if (PORT->PORTDISABLED == 0)
			{
				unsigned char * ptr = ENCBUFF;

				*(ptr++) = FEND;
				*(ptr++) = KISS->OURCTRL | 1;
				*(ptr++) = (UCHAR)PORT->PORTTXDELAY;
				*(ptr++) = FEND;

				*(ptr++) = FEND;
				*(ptr++) = KISS->OURCTRL | 2;
				*(ptr++) = PORT->PORTPERSISTANCE;
				*(ptr++) = FEND;

				*(ptr++) = FEND;
				*(ptr++) = KISS->OURCTRL | 3;
				*(ptr++) = PORT->PORTSLOTTIME;
				*(ptr++) = FEND;

				*(ptr++) = FEND;
				*(ptr++) = KISS->OURCTRL | 4;
				*(ptr++) = PORT->PORTTAILTIME;
				*(ptr++) = FEND;

				*(ptr++) = FEND;
				*(ptr++) = KISS->OURCTRL | 5;
				*(ptr++) = PORT->FULLDUPLEX;
				*(ptr++) = FEND;
	
				PORT = (struct PORTCONTROL *)KISS->FIRSTPORT;			// ALL FRAMES GO ON SAME Q

				ASYSEND(PORT, ENCBUFF, ptr - &ENCBUFF[0]);
			}
			KISS->PORT.PARAMTIMER = TICKS*60*5;		// 5 MINS
		}
	}

	//	IF FRAMES QUEUED, AND NOT SENDING, START

	if (KISS == KISS->FIRSTPORT)					// ALL FRAMES GO ON SAME Q
	{
		//	SEE IF POLL HAS TIMED OUT

		if (PORT->KISSFLAGS & POLLINGKISS)
		{
			if (KISS->POLLFLAG)			// TIMING OUT OR RECEIVING
			{
				KISS->POLLFLAG--;

				if (KISS->POLLFLAG == 0)
				{
					//	POLL HAS TIMED OUT - MAY NEED TO DO SOMETHING

					KISS->POLLPOINTER->PORT.L2URUNC++;	// PUT IN UNDERRUNS FIELD
				}
			}
		}
	}
/*
;
;	WAITING FOR CTS
;
;	MOV	DX,MSR[EBX]
;	IN	AL,DX
;	TEST	AL,CTSBIT
;	JNZ SHORT TIMERSEND		; OK TO SEND NOW
;
*/


//	SEE IF ANYTHING TO SEND

	if ((PORT->KISSFLAGS & POLLEDKISS) == 0 || KISS->POLLED)
	{
		// OK to Send

		if (KISS->KISSTX_Q)
		{
			//	IF NETROM MODE AND NOT FULL DUP, CHECK DCD

			KISS->POLLED = 0;
			
			//CMP	PROTOCOL[EBX],2		; NETROM?
			//JNE SHORT DONTCHECKDCD_1

			//CMP	FULLDUPLEX[EBX],1
			//JE SHORT DONTCHECKDCD_1
			//TEST	AL,CTSBIT		; DCD HIGH?
			//	JZ SHORT NOTHINGTOSEND		; NO, SO WAIT

			//	DROP RTS TO LOCK OUT OTHERS

			//	MOV	DX,MCR[EBX]
			//	MOV	AL,09H			; DTR OUT2

			//	OUT	DX,AL


			//	MAKE SURE CTS IS STILL ACTIVE - IF NOT, WE HAVE A COLLISION,
			//	SO RELEASE RTS AND WAIT

			//	DELAY

			//	MOV	DX,MSR[EBX]
			//	IN	AL,DX
			//	TEST	AL,CTSBIT
			//	JNZ SHORT TIMERSEND		; STILL HIGH, SO CAN SEND

			//	RAISE RTS AGAIN, AND WAIT A BIT MORE

			//	DELAY
	
			//MOV	DX,MCR[EBX]
			//	MOV	AL,0BH			; RTS DTR OUT2

			//	OUT	DX,AL

	
			Buffer = Q_REM(&KISS->KISSTX_Q);
			SENDFRAME(KISS, Buffer);
			return;
		}
	}

	// Nothing to send. IF POLLED MODE, SEND A POLL TO NEXT PORT

	if ((PORT->KISSFLAGS & POLLINGKISS) && KISS->FIRSTPORT->POLLFLAG == 0)
	{
		struct KISSINFO * POLLTHISONE;

		KISS = KISS->FIRSTPORT;	// POLLPOINTER is in first port

		//	FIND WHICH CHANNEL TO POLL NEXT

		POLLTHISONE = KISS->POLLPOINTER->SUBCHAIN;	// Next to poll
		
		if (POLLTHISONE == NULL)
			POLLTHISONE = KISS;			// Back to start
	
		KISS->POLLPOINTER = POLLTHISONE;	// FOR NEXT TIME

		KISS->POLLFLAG = TICKS / 2;			// ALLOW 1/3 SEC 

		ENCBUFF[0] = FEND;
		ENCBUFF[1] = POLLTHISONE->OURCTRL | 0x0e;	// Poll
		ENCBUFF[2] = FEND;

		ASYSEND((struct PORTCONTROL *)KISS, ENCBUFF, 3);
	}

	return;
}

int KISSRX(struct KISSINFO * KISS)
{
	struct PORTCONTROL * PORT = (struct PORTCONTROL *)KISS;
	UCHAR * Buffer;
	int len;
	NPASYINFO Port = KISSInfo[PORT->PORTNUMBER];
	struct KISSINFO * SAVEKISS = KISS;		// Save so we can restore at SeeifMode

	if (Port == NULL)
		return 0;

SeeifMore:

	KISS = SAVEKISS;

	if (KISS == 0)
		return 0;								// Just in case

	if (!Port->MSGREADY)
		CheckReceivedData(PORT, Port);		// Look for data in RXBUFFER and COM port

	if (!Port->MSGREADY)
		return 0;

	// Have a KISS frame
	
	len = Port->RXMPTR - &Port->RXMSG[0];

	// reset pointers

	Port->MSGREADY = FALSE;
	Port->RXMPTR = (UCHAR *)&Port->RXMSG;

	if (len > 329)			// Max ax.25 frame + KISS Ctrl
	{
		if (Port->Portvector)
			Debugprintf("BPQ32 overlong KISS frame - len = %d Port %d", len, Port->Portvector->PORTNUMBER);
		return 0;
	}


//	IF NETROM, CAN PASS ON NOW

	if (PORT->PROTOCOL == 2)
	{
		Buffer = (UCHAR *)GetBuff();
		
		if (Buffer)
		{
			memcpy(&Buffer[7], &Port->RXMSG[0], len);
			len += 7;

			PutLengthinBuffer(Buffer, len);
//			Buffer[5] = (len & 0xff);
//			Buffer[6] = (len >> 8);

			C_Q_ADD(&PORT->PORTRX_Q, (UINT *)Buffer);
		}

		return 0;
	}

	//	Any response should clear POLL OUTSTANDING

	KISS->POLLFLAG = 0;			// CLEAR POLL OUTSTANDING

	// See if message is a poll (or poll response)

	if ((Port->RXMSG[0] & 0x0f) == 0x0e)		// POLL Frame
	{
		int PolledPort;
		
		if (PORT->KISSFLAGS & POLLINGKISS)
		{
			// Make Sure response is from the device I polled

			if ((Port->RXMSG[0] & 0xf0) == KISS->POLLPOINTER->OURCTRL)
			{
				// if Nothing queued for tx, poll again (to speed up response)

				if (KISS->KISSTX_Q == 0)
				{
					struct KISSINFO * POLLTHISONE;

					//	FIND WHICH CHANNEL TO POLL NEXT

					POLLTHISONE = KISS->POLLPOINTER->SUBCHAIN;	// Next to poll
		
					if (POLLTHISONE == NULL)
						POLLTHISONE = KISS;			// Back to start
	
					KISS->POLLPOINTER = POLLTHISONE;	// FOR NEXT TIME

					KISS->POLLFLAG = TICKS / 2;			// ALLOW 1/3 SEC 

					ENCBUFF[0] = FEND;
					ENCBUFF[1] = POLLTHISONE->OURCTRL | 0x0e;	// Poll
					ENCBUFF[2] = FEND;

					ASYSEND((struct PORTCONTROL *)KISS, ENCBUFF, 3);
				}	
			}
			else
				Debugprintf("Polled KISS - response from wrong address - Polled %d Reponse %d",  
					KISS->POLLPOINTER->OURCTRL, (Port->RXMSG[0] & 0xf0));

			goto SeeifMore;				// SEE IF ANYTHING ELSE
		}

		//	WE ARE A SLAVE, AND THIS IS A POLL. SEE IF FOR US, AND IF SO, REPLY

		PolledPort = Port->RXMSG[0] & 0xf0;

		while (KISS->OURCTRL != PolledPort)
		{
			KISS = KISS->SUBCHAIN;
			if (KISS == NULL)
				goto SeeifMore;				// SEE IF ANYTHING ELSE
		}

		//	SEE IF ANYTHING QUEUED

		if (KISS->KISSTX_Q)
		{
			KISS->POLLED = 1;			// LET TIMER DO THE SEND
			goto SeeifMore;				// SEE IF ANYTHING ELSE
		}

		ENCBUFF[0] = FEND;
		ENCBUFF[1] = KISS->OURCTRL | 0x0e;	// Poll/Poll Resp
		ENCBUFF[2] = FEND;

		ASYSEND(PORT, ENCBUFF, 3);
		goto SeeifMore;				// SEE IF ANYTHING ELSE
	}

	//	MESSAGE MAY BE DATA OR DATA ACK. IT HAS NOT YET BEEN CHECKSUMMED


	if ((Port->RXMSG[0] & 0x0f) == 0x0c)		// ACK Frame
	{
		//	ACK FRAME. WE DONT SUPPORT ACK REQUIRED FRAMES AS A SLAVE - THEY ARE ONLY ACCEPTED BY TNCS

		struct _LINKTABLE * LINK;
		UINT ACKWORD = Port->RXMSG[1] | Port->RXMSG[2] << 8;

		ACKWORD += (UINT)LINKS;
		LINK = (struct _LINKTABLE *)ACKWORD;

		if (LINK->L2TIMER)
			LINK->L2TIMER = LINK->L2TIME;

		return 0;
	}

	if (Port->RXMSG[0] & 0x0f)		// Not Dats 
	{
		Port->RXMSG[len] = 0;
/*
RSIDN:1504,PSK250C6,1499,PSK250C6,ACTIVE
TXBE:Q:PSK250C6.78.Kiss
RSIDN:1504,PSK250C6,1504,PSK250C6,ACTIVE
*/
//		Debugprintf(Port->RXMSG);
		return 0;
	}

	//	checksum if necessary

	if (len < 15)
		return 0;					// too short for AX25

	if (PORT->KISSFLAGS & CHECKSUM)
	{
		//	SUM MESSAGE, AND IF DUFF DISCARD. IF OK DECREMENT COUNT TO REMOVE SUM

		int sumlen = len;
		char * ptr = &Port->RXMSG[0];
		UCHAR sum = 0;

		while (sumlen--)
		{
			sum ^= *(ptr++);
		}

		if (sum)
		{
			PORT->RXERRORS++;
			Debugprintf("KISS Checksum Error");

			goto SeeifMore;				// SEE IF ANYTHING ELSE
		}
		len--;							// Remove Checksum
	}

	//	FIND CORRECT SUBPORT RECORD
	
	while (KISS->OURCTRL != Port->RXMSG[0])
	{
		KISS = KISS->SUBCHAIN;

		if (KISS == NULL)
			goto SeeifMore;				// SEE IF ANYTHING ELSE
	}
	
	//	ok, KISS now points to our port

	Buffer = (UCHAR *)GetBuff();
		
	// we dont need the control byte
	
	len --;
	
	if (Buffer)
	{
		memcpy(&Buffer[7], &Port->RXMSG[1], len);
		len += 7;

		PutLengthinBuffer(Buffer, len);		// Neded for arm5 portability

//		Buffer[5] = (len & 0xff);
//		Buffer[6] = (len >> 8);

		C_Q_ADD(&KISS->PORT.PORTRX_Q, (UINT *)Buffer);
	}

	goto SeeifMore;				// SEE IF ANYTHING ELSE
}

#ifndef WIN32

int i2cPoll(struct PORTCONTROL * PORT, NPASYINFO npKISSINFO)
{
	unsigned int retval;
	int len;
	UCHAR * ptr;
	int fd = npKISSINFO->idComDev;

	retval = i2c_smbus_read_byte(fd);
	
	//	Returns POLL (0x0e) if nothing to receive, otherwise the control byte of a frame
	
	if (retval == -1)	 		// Read failed		
  	{
		perror("poll failed");
		return 0;
	}
		
//	NACK means last message send to TNC was duff

	if (retval == 0x15)			// NACK
	{
		int i = lastcount;
		UCHAR * ptr = lastblock;
		int ret;
		
		while (i--)
		{
			ret = i2c_smbus_write_byte(fd, *(ptr++));
			if (ret == -1)
			{
				Debugprintf ("i2c Write Error\r");
				usleep(1000);
				ret = i2c_smbus_write_byte(fd, *(ptr++));
			}		
		}
		Debugprintf ("i2c Block resent %d\n", lastcount);
		return 0;
	}

	if (retval == 0x0e)
	{
		struct KISSINFO * KISS = (struct KISSINFO *)PORT;
		KISS->TXACTIVE = FALSE;

		return 0;
	}

	// 	Read message up to a FEND into &npKISSINFO->RXBUFFER[0]

	ptr = &npKISSINFO->RXBUFFER[0];

	// First is FEND, which we don't really need

	*(ptr++) = retval;				// Put first char in buffer
	len = 1;

	while (retval != FEND || len < 2)
	{
		usleep(1000);
		
		retval = i2c_smbus_read_byte(fd);
			
		if (retval == -1)	 		// Read failed		
	  	{
			perror("poll failed in packet loop");	
			return 0;
		}
		
		*(ptr++) = retval;
		len ++;

		if (len > 500)
		{
			Debugprintf ("i2c oversized block\n");
			return 0;
		}
	}

	return len;
}
#endif

// UZ7HO KISS Over TCP Routines

VOID ConnecttoUZ7HOTCPThread(NPASYINFO ASY);

int ConnecttoUZ7HOTCP(NPASYINFO ASY)
{
	_beginthread(ConnecttoUZ7HOTCPThread, 0, ASY);

	return 0;
}

VOID ConnecttoUZ7HOTCPThread(NPASYINFO ASY)
{
	char Msg[255];
	int err,i;
	u_long param=1;
	BOOL bcopt=TRUE;
	SOCKET sock;
//	struct hostent * HostEnt;
	SOCKADDR_IN sinx; 
	int addrlen=sizeof(sinx);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	//	Only called for the first BPQ port for a particular host/port combination

	Sleep(10000);		// Delay a bit

	while(1)
	{
		if (ASY->Connected == FALSE && ASY->Connecting == FALSE)
		{
//			if (ASY->destaddr.s_addr == INADDR_NONE)
//			{
				//	Resolve name to address

//				 HostEnt = gethostbyname (AGWHostName[port]);
		 
//				 if (!HostEnt) return;			// Resolve failed

//				 memcpy(&destaddr[port].sin_addr.s_addr,HostEnt->h_addr,4);

//			}

			sock = ASY->sock = socket(AF_INET, SOCK_STREAM, 0);

			if (sock == INVALID_SOCKET)
			{
				i=sprintf(Msg, "Socket Failed for KISSTCP socket - error code = %d\r\n", WSAGetLastError());
				WritetoConsoleLocal(Msg);
		 	 	return; 
			}
 
			setsockopt (sock, SOL_SOCKET,SO_REUSEADDR, (const char FAR *)&bcopt, 4);

			if (bind(sock, (LPSOCKADDR) &sinx, addrlen) != 0 )
			{
				//	Bind Failed
	
				i=sprintf(Msg, "Bind Failed for KISSTCP socket - error code = %d\r\n", WSAGetLastError());
				WritetoConsoleLocal(Msg);

				closesocket(sock);
		 	 	return; 
			}

			ASY->Connecting = TRUE;

			if (connect(sock,(LPSOCKADDR) &ASY->destaddr, sizeof(ASY->destaddr)) == 0)
			{
				//	Connected successful

				ASY->Connected = TRUE;
				ASY->Connecting = FALSE;

				ioctlsocket (sock, FIONBIO, &param);
				continue;
			}
				else
			{
				err=WSAGetLastError();

				//	Connect failed

				if (ASY->Alerted == FALSE)
				{
					sprintf(Msg, "Connect Failed for KISSTCP Port %d - error code = %d\n",
						ASY->Portvector->PORTNUMBER, err);
				    WritetoConsoleLocal(Msg);
					ASY->Alerted = TRUE;
				}

				closesocket(sock);
				ASY->Connecting = FALSE;
				Sleep (57000);				// 1 Mins
				continue;
			}
		}
		Sleep (57000);						// 1 Mins
	}
}

int KISSGetTCPMessage(NPASYINFO ASY)
{
	int index=0;
	ULONG param = 1;

	if (ASY->Listening)
	{
		//	TCP Slave waiting for a connection

		SOCKET sock;
		int addrlen = sizeof(struct sockaddr_in);
		struct sockaddr_in sin;  

		sock = accept(ASY->sock, (struct sockaddr *)&sin, &addrlen);

		if (sock == INVALID_SOCKET)
		{
			int err = GetLastError();

			if (err == 10035 || err == 11)		// Would Block
				return 0;

		}
		
		//	Have a connection. Close Listening Socket and use new one

		closesocket(ASY->sock);

		ioctl(sock, FIONBIO, &param);
		ASY->sock = sock;
		ASY->Listening = FALSE;
		ASY->Connected = TRUE;
	}

	if (ASY->Connected)
	{
		int InputLen;

		//	Poll TCP COnnection for data

		// May have several messages per packet, or message split over packets

		InputLen = recv(ASY->sock, ASY->RXBUFFER, MAXBLOCK - 1, 0);

		if (InputLen < 0)
		{
			int err = WSAGetLastError();

			if (err == 10035 || err == 11)
				InputLen = 0;
				return 0;

			ASY->Connected = 0;
			closesocket(ASY->sock);
			return 0;
		}

		if (InputLen > 0)
			return InputLen;
		else
		{
			Debugprintf("KISSTCP Close received for socket %d", ASY->sock);

			ASY->Connected = 0;
			closesocket(ASY->sock);

			if (ASY->Portvector->KISSSLAVE)
			{
				// Reopen Listening Socket

				SOCKET sock;
				u_long param=1;
				BOOL bcopt=TRUE;
				struct sockaddr_in sinx;

				ASY->sock = sock = socket(AF_INET,SOCK_STREAM,0);
				ioctl(sock, FIONBIO, &param);

				sinx.sin_family = AF_INET;
				sinx.sin_addr.s_addr = INADDR_ANY;		
				sinx.sin_port = htons(ASY->Portvector->ListenPort);

				if (bind(sock, (struct sockaddr *) &sinx, sizeof(sinx)) != 0 )
				{
					//	Bind Failed

					int err = WSAGetLastError();
					Consoleprintf("Bind Failed for KISS TCP port %d - error code = %d", ASY->Portvector->ListenPort, err);
					closesocket(sock);
				}
				else
				{
					if (listen(sock, 1) < 0)
					{
						int err = WSAGetLastError();
						Consoleprintf("Listen Failed for KISS TCP port %d - error code = %d", ASY->Portvector->ListenPort, err);
						closesocket(sock);
					}
					else
						ASY->Listening = TRUE;	
				}

			}
			return 0;
		}
	}
	return 0;
}
