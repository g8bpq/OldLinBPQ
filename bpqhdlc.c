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
//	Module to provide HDLC Card (DRSI, Baycom etc) support for
//	G8BPQ switch in a 32bit environment

//
//	Win95 - Uses BPQHDLC.VXD to drive card
//  NT -Uses BPQHDLC.DRV to drive card
//
#define _CRT_SECURE_NO_DEPRECATE 

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "CHeaders.h"
#include "bpq32.h"


extern UINT TRACE_Q;

_CRT_OBSOLETE(GetVersionEx) errno_t __cdecl _get_winmajor(__out unsigned int * _Value);
_CRT_OBSOLETE(GetVersionEx) errno_t __cdecl _get_winminor(__out unsigned int * _Value);


#define FILE_DEVICE_BPQHDLC			0x00008421

#define IOCTL_BPQHDLC_SEND			CTL_CODE(FILE_DEVICE_BPQHDLC,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQHDLC_POLL			CTL_CODE(FILE_DEVICE_BPQHDLC,0x801,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQHDLC_TIMER			CTL_CODE(FILE_DEVICE_BPQHDLC,0x802,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQHDLC_ADDCHANNEL	CTL_CODE(FILE_DEVICE_BPQHDLC,0x803,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQHDLC_CHECKTX		CTL_CODE(FILE_DEVICE_BPQHDLC,0x804,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQHDLC_IOREAD		CTL_CODE(FILE_DEVICE_BPQHDLC,0x805,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQHDLC_IOWRITE		CTL_CODE(FILE_DEVICE_BPQHDLC,0x806,METHOD_BUFFERED,FILE_ANY_ACCESS)


VOID __cdecl Debugprintf(const char * format, ...);


//	Info to pass to Kernel HDLC Driver to define an SCC Subchannel

typedef struct _BPQHDLC_ADDCHANNEL_INPUT {

	ULONG   IOBASE;			// IO Base Address
    ULONG	IOLEN;			// Number of Addresses
    UCHAR   Interrupt;		// Interrupt
	UCHAR	Channel;

	ULONG	ASIOC;			// A CHAN ADDRESSES
	ULONG	SIO;			// OUR ADDRESSES (COULD BE A OR B) 
	ULONG	SIOC;			// Our Control Channel 
	ULONG	BSIOC;			//  B CHAN CONTROL

	VOID * OtherChannel;		// Kernel Channel record for first channel if this is 2nd channel

	UCHAR SOFTDCDFLAG;			// Use SoftDCD flag

	int TXBRG;				// FOR CARDS WITHOUT /32 DIVIDER
	int RXBRG;

	UCHAR WR10;				// NRZ/NRZI FLAG
 
	USHORT TXDELAY;			//TX KEYUP DELAY TIMER
	UCHAR PERSISTANCE;

}  BPQHDLC_ADDCHANNEL_INPUT, *PBPQHDLC_ADDCHANNEL_INPUT;


DWORD n;

HANDLE hDevice=0;
BYTE bOutput[4]=" ";
DWORD cb=0;
int fResult=0;

BOOL Win98 = FALSE;


extern int QCOUNT;
int Init98(HDLCDATA * PORTVEC);
int Init2K(HDLCDATA * PORTVEC);
int INITPORT(PHDLCDATA PORTVEC);



int HDLCRX2K(PHDLCDATA PORTVEC, UCHAR * buff)
{
	ULONG Param;
	DWORD len=0;

	if (hDevice == 0)
		return (0);


	if (PORTVEC->DRIVERPORTTABLE == 0) return 0;

	memcpy(&Param, &PORTVEC->DRIVERPORTTABLE,4);

	fResult = DeviceIoControl(
			hDevice,   // device handle
			(Win98) ? 'G' : IOCTL_BPQHDLC_POLL,		   // control code
			&Param, (Win98) ? (rand() & 0xff) : 4, //Input Params
	        buff,360,&len, // output parameters
			0);

	return (len);
}

int HDLCTIMER2K(PHDLCDATA  PORTVEC)
{
	DWORD len=0;

	if (hDevice == 0)
		return (0);

	if (PORTVEC->DRIVERPORTTABLE == 0) return 0;

	fResult = DeviceIoControl(
			hDevice,   // device handle
			(Win98) ? 'T' : IOCTL_BPQHDLC_TIMER,		   // control code
			&PORTVEC->DRIVERPORTTABLE,4, //Input Params
	        0,0,&len, // output parameters
			0);

	return (0);
}
 int HDLCTXCHECK2K(PHDLCDATA PORTVEC)
 {
	DWORD Buff;
	DWORD len=0;

	if (hDevice == 0)
		return (0);

	if (Win98)
		return 0;

	if (PORTVEC->DRIVERPORTTABLE == 0) return 0;

	fResult = DeviceIoControl(
			hDevice,   // device handle
			IOCTL_BPQHDLC_CHECKTX,		   // control code
			&PORTVEC->DRIVERPORTTABLE,4, //Input Params
	        &Buff,4,&len, // output parameters
			0);

	return (Buff);
}



int HDLCTX2K(PHDLCDATA  PORTVEC,UCHAR * buff)
{
	DWORD txlen=0;

	if (hDevice == 0)
		return (0);

	txlen=(buff[6]<<8) + buff[5];
	
	memcpy(buff,&PORTVEC->DRIVERPORTTABLE,4);
	
	fResult = DeviceIoControl(
			hDevice,   // device handle
			(Win98) ? 'S' : IOCTL_BPQHDLC_SEND,	   // control code
		   // control code
			buff,txlen,	// input parameters
	        NULL,0,&cb, // output parameters
			0);

	return (0);
}


int HDLCCLOSE(PHDLCDATA PORTVEC)
{
	if (hDevice)
	{
		CloseHandle(hDevice);
		hDevice = 0;
	}

	return 0;
}

int HDLCRX98(PHDLCDATA PORTVEC, UCHAR * buff)
{
	DWORD len=0;

	if (hDevice == 0)
		return (0);

	fResult = DeviceIoControl(
			hDevice,   // device handle
			'G',		   // control code
			PORTVEC->DRIVERPORTTABLE,rand() & 0xff, //Input Params
	        buff,360,&len, // output parameters
			0);

	return (len);
}

int HDLCTIMER98(PHDLCDATA PORTVEC)
{
	DWORD len=0;

	if (hDevice == 0)
		return (0);

	fResult = DeviceIoControl(
			hDevice,   // device handle
			'T',		   // control code
			PORTVEC->DRIVERPORTTABLE,4, //Input Params
	        0,0,&len, // output parameters
			0);

	return (0);
}

 int HDLCTXCHECK98(PHDLCDATA PORTVEC)
 {
	 return 0;
 }

int HDLCTX98(PHDLCDATA PORTVEC,UCHAR * buff)
{
	DWORD txlen=0;

	if (hDevice == 0)
		return (0);

	txlen=(buff[6]<<8) + buff[5];
	
	memcpy(buff,&PORTVEC->DRIVERPORTTABLE,4);
	
	fResult = DeviceIoControl(
			hDevice,   // device handle
			'S',		   // control code
			buff,txlen,// input parameters
	        NULL,0,&cb, // output parameters
			0);

	return (0);
}

int IntHDLCRX(PHDLCDATA PORTVEC, UCHAR * buff)
{
	if (Win98)
		return HDLCRX98(PORTVEC, buff);
	else
		return HDLCRX2K(PORTVEC, buff);
}

VOID HDLCRX(PHDLCDATA PORTVEC)
{
	struct _MESSAGE * Message;
	int Len;
	struct PORTCONTROL * PORT = (struct PORTCONTROL *)PORTVEC;

	if (QCOUNT < 10)
		return;

	Message = GetBuff();

	if (Message == NULL)
		return;

	Len = IntHDLCRX(PORTVEC, (UCHAR *)Message);
	
	if (Len == 0)
	{
		ReleaseBuffer((UINT *)Message);
		return;
	}

	C_Q_ADD(&PORT->PORTRX_Q, (UINT *)Message);

	return;
}

int HDLCTIMER(PHDLCDATA PORTVEC)
{
	if (Win98)
		return HDLCTIMER98(PORTVEC);
	else
		return HDLCTIMER2K(PORTVEC);
}

int HDLCTXCHECK(PHDLCDATA PORTVEC)
{
	if (Win98)
		return HDLCTXCHECK98(PORTVEC);
	else
		return HDLCTXCHECK2K(PORTVEC);
}



VOID HDLCTX(PHDLCDATA PORTVEC,UCHAR * Buffer)
{
	struct _LINKTABLE * LINK;
	
	LINK = (struct _LINKTABLE *)Buffer[(BUFFLEN-4)/4];

	if (LINK)
	{
		if (LINK->L2TIMER)
			LINK->L2TIMER = LINK->L2TIME;

		Buffer[(BUFFLEN-4)/4] = 0;	// CLEAR FLAG FROM BUFFER
	}


	if (Win98)
		HDLCTX98(PORTVEC, Buffer);
	else
		HDLCTX2K(PORTVEC, Buffer);
	
	C_Q_ADD(&TRACE_Q, (UINT *)Buffer);

}

int HDLCINIT(HDLCDATA * PORTVEC)
{
	int WinVer, WinMinor;

	WritetoConsole("HDLC\n");

#pragma warning(push)
#pragma warning(disable : 4996)

	_get_winmajor(&WinVer);
	_get_winminor(&WinMinor);

#pragma warning(pop)
	
	if (WinVer >= 5)		// Win 2000 or above
		return Init2K(PORTVEC);
	else
	{
		Init98(PORTVEC);
		OutputDebugString("HDLC Win98 Return from Init98\n");
		return 0;
	}

}

int Init98(HDLCDATA * PORTVEC)
{
	char msg[255];
	int err;

	Win98 = TRUE;
	
	OutputDebugString("Init HDLC 98\n");

	//
	//	Open HDLC Driver, send send config params
	//

	if (hDevice == 0)		// Not already loaded
	{
		//
		//	Load VXD
		//

		hDevice = CreateFile("\\\\.\\BPQHDLC.VXD",
			0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);

		if (hDevice == INVALID_HANDLE_VALUE)
		{
			hDevice=0;

			err=GetLastError();
	
			sprintf(msg,"Error loading Driver \\\\.\\BPQHDLC.VXD - Error code %d\n",err);
			OutputDebugString(msg);
			
			MessageBox(NULL,msg,"BPQ32",MB_ICONSTOP);

			WritetoConsole("Initialisation Failed");

			return (FALSE);
		}

//		OutputDebugString("Calling GetVersion\n");

//		fResult = DeviceIoControl(
//			hDevice,          // device handle
//			10,//DIOC_GETVERSION,  // control code
//			NULL,0,// input parameters
//			bOutput, 4, &cb,  // output parameters
//			0);

		srand( (unsigned)time( NULL ) );  //Prime random no generator	
	}

	OutputDebugString("Calling Initialize\n");

	//
	//	Initialize Driver for this card and channel
	//

	fResult = DeviceIoControl(
		hDevice,						// device handle
		'I',							// control code
		PORTVEC, sizeof (struct PORTCONTROL),	// input parameters
		bOutput, 4, &cb,				// output parameters
        0);


	memcpy(&PORTVEC->DRIVERPORTTABLE,bOutput,4);

	Debugprintf("BPQ32 HDLC Driver Table ADDR %X", PORTVEC->DRIVERPORTTABLE);

	OutputDebugString("Initialize Returned\n");

	return (TRUE);
		
}


int PC120INIT(PHDLCDATA PORTVEC)
{
	return (HDLCINIT(PORTVEC));
}

int DRSIINIT(PHDLCDATA PORTVEC)
{
	return (HDLCINIT(PORTVEC));
}

int TOSHINIT(PHDLCDATA PORTVEC)
{
	return (HDLCINIT(PORTVEC));
}

int RLC100INIT(PHDLCDATA PORTVEC)
{
	return (HDLCINIT(PORTVEC));
}

int BAYCOMINIT(PHDLCDATA PORTVEC)
{
	return (HDLCINIT(PORTVEC));
}

int PA0INIT(PHDLCDATA PORTVEC)		// 14 PA0HZP OPTO-SCC
{
	return (HDLCINIT(PORTVEC));
}

// W2K/XP Routines

#define IOTXCA VECTOR[0]
#define IOTXEA VECTOR[1]
#define IORXCA VECTOR[2]
#define IORXEA VECTOR[3]

#define SIOR READ_PORT_UCHAR(PORTVEC->SIO)
#define SIOW(A) WRITE_PORT_UCHAR(PORTVEC->SIO,A)

#define SIOCR READ_PORT_UCHAR(PORTVEC->SIOC)
#define SIOCW(A) WRITE_PORT_UCHAR(PORTVEC->SIOC, A)

//#define SETRVEC	PORTVEC->IORXCA =
//#define SETTVEC	PORTVEC->IOTXCA =


int CLOCKFREQ = 76800;			// 4,915,200 HZ /(32*2)

int TOSHCLOCKFREQ =	57600;

UCHAR SDLCCMD[]	= {
	0,0,
	2,0,			// BASE VECTOR 
	4,0x20,			// SDLC MODE
	3,0xc8,			// 8BIT,  CRC ENABLE, RX DISABLED

	7,0x7e,			// STANDARD FLAGS
	1,0x13,			// INT ON ALL RX, TX INT EN, EXT INT EN
	5,0xe1,			// DTR, 8BIT, SDLC CRC,TX CRC EN

	10,0xa0,		// CRC PRESET TO 1

	9,0x09,			// ENABLE INTS

	11,0x66,		// NO XTAL, RXC = DPLL, TXC = RTXC, TRXC = BRG (NEEDS /32 BETWEEN TRXC AND RTXC)

	14,0x83,
	14,0x23,
	15,0xc0			// EXT INT ONLY ON TX UND AND ABORT RX
};

#define SDLCLEN	26

UCHAR TOSHR11 = 0x68;	// NO XTAL, RXC = DPLL, TXC = DPLL, NO CLK OUTPUT

UCHAR CIOPARAMS[] = {

	0x2B,0xFF,		// B DIRECTION - ALL IN
	0x23,0xFF,		// A DIRECTION - ALL IN

	0x1D,0x0E2,		// C/T 2 MODE - CONT, EXT IN, EXT O, SQUARE
	0x1C,0x0E2,		// C/T 1 MODE   		"" 

	0x19,0x10,		// C/T 2 LSB - 16 = /32 FOR SQUARE WAVE
	0x18,0,			//       MSB

	0x17,0x10,		// C/T 1 LSB
	0x16,0,			//       MSB

	0x0B,0x04,		// CT2   ""    - GATE
	0x0A,0x04,		// CT1   ""    - GATE

	0x06,0x0F,		// PORT C DIRECTION - INPUTS

	1,0x84,			// ENABLE PORTS A AND B

	0,0				// INTERRUPT CONTROL
};

#define CIOLEN	26


VOID WRITE_PORT_UCHAR(UINT Port, UINT Value)
{
  	ULONG buff[3];
	
	buff[0] = Port;
	buff[1] = Value;
	
	fResult = DeviceIoControl(
			hDevice,   // device handle
			IOCTL_BPQHDLC_IOWRITE,	   // control code
			buff, 8,	// input parameters
	        NULL,0,&cb, // output parameters
			0);
}

UCHAR READ_PORT_UCHAR(ULONG Port)
{
	ULONG buff[3];
	
	buff[0] = Port;
	
	fResult = DeviceIoControl(
			hDevice,   // device handle
			IOCTL_BPQHDLC_IOREAD,	   // control code
			buff, 4,	// input parameters
	        buff, 4,&cb, // output parameters
			0);

	Debugprintf("BPQ32 HDLC READ_PORT_UCHAR Returned  %X", LOBYTE(buff[0]));

	return LOBYTE(buff[0]);

}

int Init2K(HDLCDATA * PORTVEC)
{
	char msg[255];
	int err;

	if (hDevice == 0)		// Not already loaded
	{
		//
		//	Open HDLC Driver
		//
		
		hDevice  = CreateFile(
                    "\\\\.\\BPQHDLC",           // Open the Device "file"
                    GENERIC_WRITE,
                    FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL);


		if (hDevice == INVALID_HANDLE_VALUE)
		{
			hDevice=0;

			err=GetLastError();
	
			sprintf(msg,"Error Opening Driver \\device\\BPQHDLC - Error code %d\n", err);
			OutputDebugString(msg);

			WritetoConsole(msg);

			return (FALSE);
		}
	}

	INITPORT(PORTVEC);

	return 0;
}

PHDLCDATA See_if_First_On_Card(PHDLCDATA PORTVEC)
{
//	SEE IF ANOTHER PORT IS ALREADY USING THE OTHER CHANNEL ON THIS CARD
	int i;

	PHDLCDATA PreviousPort = (PHDLCDATA)PORTTABLE;

	for (i = 0; i < NUMBEROFPORTS; i++)
	{
		if (PORTVEC == PreviousPort)
		{
			// NONE BEFORE OURS

			return NULL;
		}

		if (PORTVEC->PORTCONTROL.IOBASE == PreviousPort->PORTCONTROL.IOBASE)
		{
			//	ENSURE ENTRIES ARE FOR DIFFERENT CHANNELS

			if (PORTVEC->PORTCONTROL.CHANNELNUM == PreviousPort->PORTCONTROL.CHANNELNUM)
		
				//	CHANNEL DEFINITION ERROR

				return (PHDLCDATA) -1;
			else
				return PreviousPort;
		}
	
		PreviousPort = (PHDLCDATA)PreviousPort->PORTCONTROL.PORTPOINTER;
	}
	
	return NULL;			// FLAG NOT FOUND




}

VOID INITPART2(PHDLCDATA PORTVEC, USHORT SCCOffset, PHDLCDATA PreviousPort)
{
//	SCCOffset is address of SCC relative to Card Base Address

	int i;
	USHORT SCCBase=PORTVEC->PORTCONTROL.IOBASE + SCCOffset;
	int BRG;

//	SET UP ADDRESS LIST - THIS PATH FOR CARDS WITH 'NORMAL'
//	ADDRESSING - C/D=A0, A/B=A1, SO ORDER IS BCTRL BDATA ACTRL ADATA
//	OR DE, WHICH USES WORD ADDRESSES C/D=A1, A/B=A2 

	PORTVEC->BSIOC = SCCBase;			// B CHAN ADDR
	PORTVEC->ASIOC = SCCBase+2;		// A CHAN ADDR
 
//	SEE WHICH CHANNEL TO USE

	if (PORTVEC->PORTCONTROL.CHANNELNUM == 'A')
	{
		PORTVEC->A_PTR = PORTVEC;		// POINT TO OUR ENTRY
		PORTVEC->SIOC = SCCBase+2;
		PORTVEC->SIO = SCCBase+3;			// DATA 1 ABOVE CONTROL

		if (PreviousPort)			// Another Channel is first on Card
			PORTVEC->B_PTR = PreviousPort;	// CROSSLINK CHANNELS
	}
	else
	{
		// MUST BE B - CHECKED EARLIER

		PORTVEC->B_PTR = PORTVEC;		// POINT TO OUR ENTRY
		PORTVEC->SIOC = SCCBase;
		PORTVEC->SIO = SCCBase+1;	// DATA 1 ABOVE CONTROL

		if (PreviousPort)			// Another Channel is first on Card
			PORTVEC->A_PTR = PreviousPort;	// CROSSLINK CHANNELS

	}

//	INITIALISE COMMS CHIP

	if (PreviousPort == 0)					// OTHER CHAN ALREADY SET UP?
	{
		// DO A HARD RESET OF THE SCC

		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 0);		// Make Sure WR0
		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 0);

		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 9);		// WR9

		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 0xC0);	// Hard Reset

		Sleep(2);

	}

	for (i=0; i< SDLCLEN; i++)
	{
		WRITE_PORT_UCHAR(PORTVEC->SIOC, SDLCCMD[i]);
	}

	PORTVEC->WR10 = 0x20;			// NRZI

//	SET UP BRG FOR REQUIRED SPEED

	if (PORTVEC->PORTCONTROL.BAUDRATE == 0)
	{
		//	SET EXTERNAL CLOCK

		SIOCW(11);			// WR11
		SIOCW(0x20);		// RX = TRXC TX = RTXC

		return;
	}

	if (PORTVEC->PORTCONTROL.PORTTYPE == 12)	//	RLC 400 USES SAME CLOCK AS TOSH

		BRG = TOSHCLOCKFREQ;
	else
		BRG = CLOCKFREQ;

	BRG=(BRG/PORTVEC->PORTCONTROL.BAUDRATE)-2;

	SIOCW(12);			// Select WR12
	SIOCW(BRG & 0xff);	// SET LSB
	SIOCW(13);			// Select WR13
	SIOCW(BRG >> 8);	// SET MSB

	return;
}

VOID INITCIO(PHDLCDATA PORTVEC)
{
//	INITIALISE CIO - DRSI ONLY
	int i;
	ULONG CIOAddr = PORTVEC->PORTCONTROL.IOBASE + 7;  // TO CIO PORT

	READ_PORT_UCHAR(CIOAddr);
	WRITE_PORT_UCHAR(CIOAddr, 0);

	READ_PORT_UCHAR(CIOAddr);
	WRITE_PORT_UCHAR(CIOAddr, 0);

	WRITE_PORT_UCHAR(CIOAddr, 1);	// FORCE RESET
	WRITE_PORT_UCHAR(CIOAddr, 0);	//  CLEAR RESET

	for (i=0; i< CIOLEN; i++)
	{
		WRITE_PORT_UCHAR(CIOAddr, CIOPARAMS[i] );
	}

	return;

}
VOID STARTCIO(PHDLCDATA PORTVEC)
{
	USHORT CIOAddr = PORTVEC->PORTCONTROL.IOBASE + 7;  // TO CIO PORT
	UCHAR Reg;

//	B CHANNEL

//	SET COUNTER OUTPUT BIT ACTIVE

	WRITE_PORT_UCHAR(CIOAddr, 0x2B);	// PORT B DIRECTION
	Reg = READ_PORT_UCHAR(CIOAddr);

	if (PORTVEC->PORTCONTROL.CHANNELNUM == 'B')
	{

		Reg &= 0xEF;						// SET BIT 4 AS OUTPUT

		WRITE_PORT_UCHAR(CIOAddr, 0x2B);	// PORT B DIRECTION
		WRITE_PORT_UCHAR(CIOAddr, Reg);		// UPDATE PORT B DIRECTION

		//	ENABLE COUNTER

		WRITE_PORT_UCHAR(CIOAddr,1);		// MASTER CONFIG
		Reg = READ_PORT_UCHAR(CIOAddr);		// GET IT

		Reg |= 0x40;						// ENABLE CT1

		WRITE_PORT_UCHAR(CIOAddr,1);		// MASTER CONFIG
		WRITE_PORT_UCHAR(CIOAddr, Reg);		// Set it

		//	START COUNTER

		WRITE_PORT_UCHAR(CIOAddr,0x0A);		// CT1 CONTROL
		WRITE_PORT_UCHAR(CIOAddr,6);		// START CT1

		return;
	}

	Reg &= 0xFE;						// SET BIT 0 AS OUTPUT

	WRITE_PORT_UCHAR(CIOAddr, 0x2B);	// PORT B DIRECTION
	WRITE_PORT_UCHAR(CIOAddr, Reg);		// UPDATE PORT B DIRECTION

	//	ENABLE COUNTER

	WRITE_PORT_UCHAR(CIOAddr,1);		// MASTER CONFIG
	Reg = READ_PORT_UCHAR(CIOAddr);		// GET IT

	Reg |= 0x20;						// ENABLE CT2

	WRITE_PORT_UCHAR(CIOAddr,1);		// MASTER CONFIG
	WRITE_PORT_UCHAR(CIOAddr, Reg);		// Set it

//	START COUNTER

	WRITE_PORT_UCHAR(CIOAddr,0x0B);		// CT2 CONTROL
	WRITE_PORT_UCHAR(CIOAddr,6);		// START CT2

	return;
}

VOID INITMODEM(PHDLCDATA PORTVEC)
{
	//	SETUP MODEM - PC120 ONLY

	WRITE_PORT_UCHAR(PORTVEC->PORTCONTROL.IOBASE, 0x0a);
}


VOID CHECKCHAN(PHDLCDATA PORTVEC, USHORT CDOffset)
{
	// CDoffset contains offset to Second SCC

	//	IF CHANNEL =  C OR D SET TO SECOND SCC ADDRESS, AND CHANGE TO A OR B

	if (PORTVEC->PORTCONTROL.CHANNELNUM > 'B')
	{
		//	SECOND SCC

		PORTVEC->PORTCONTROL.CHANNELNUM -=2;
		PORTVEC->PORTCONTROL.IOBASE+=CDOffset;
	}
}

//	BAYCOM CARD

VOID BINITPART2(PHDLCDATA PORTVEC, USHORT SCCOffset, PHDLCDATA PreviousPort)
{
	// ORDER IS 0		1		2		3		4		5		6		7
	//			ADATA	BDATA	CDATA	DDATA	ACTRL	BCTRL	CCTRL	DCTRL 

	//	Before entering here IOBASE and Chan have been updated if Chan were C or D

	//	SET UP ADDRESS LIST

	int i;
	USHORT SCCBase=PORTVEC->PORTCONTROL.IOBASE + SCCOffset;
	int BRG;

//	SET UP ADDRESS LIST - THIS PATH FOR CARDS WITH 'NORMAL'
//	ADDRESSING - C/D=A0, A/B=A1, SO ORDER IS BCTRL BDATA ACTRL ADATA
//	OR DE, WHICH USES WORD ADDRESSES C/D=A1, A/B=A2 

	PORTVEC->ASIOC = SCCBase+4;		// A CHAN ADDR
	PORTVEC->BSIOC = SCCBase+5;		// B CHAN ADDR
 
//	SEE WHICH CHANNEL TO USE

	if (PORTVEC->PORTCONTROL.CHANNELNUM == 'A')
	{
		PORTVEC->A_PTR = PORTVEC;		// POINT TO OUR ENTRY
		PORTVEC->SIOC = SCCBase+4;
		PORTVEC->SIO = SCCBase;

		if (PreviousPort)			// Another Channel is first on Card
			PORTVEC->B_PTR = PreviousPort;	// CROSSLINK CHANNELS
	}
	else
	{
		// MUST BE B - CHECKED EARLIER

		PORTVEC->B_PTR = PORTVEC;		// POINT TO OUR ENTRY
		PORTVEC->SIOC = SCCBase+5;
		PORTVEC->SIO = SCCBase+1;

		if (PreviousPort)			// Another Channel is first on Card
			PORTVEC->A_PTR = PreviousPort;	// CROSSLINK CHANNELS

	}


//	INITIALISE COMMS CHIP

	if (PreviousPort == 0)					// OTHER CHAN ALREADY SET UP?
	{
		// DO A HARD RESET OF THE SCC

		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 0);		// Make Sure WR0
		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 0);

		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 9);		// WR9

		WRITE_PORT_UCHAR(PORTVEC->ASIOC, 0xC0);	// Hard Reset

		Sleep(2);

	}

	for (i=0; i< SDLCLEN; i++)
	{
		WRITE_PORT_UCHAR(PORTVEC->SIOC, SDLCCMD[i]);
	}

//	SET UP BRG FOR REQUIRED SPEED

	if (PORTVEC->PORTCONTROL.BAUDRATE == 0)
	{
		//	SET EXTERNAL CLOCK

		SIOCW(11);			// WR11
		SIOCW(0x20);		// RX = TRXC TX = RTXC

		//	BAYCOM RUH PORT USES NRZ

		PORTVEC->WR10 = 0x0;			// NRZ

		return;
	}

	PORTVEC->WR10 = 0x20;			// NRZI

//	THERE IS NO /32 ON THE BAYCOM BOARD, SO FOR THE MOMENT WILL USE BRG
//	FOR TRANSMIT. THIS REQUIRES IT TO BE REPROGRAMMED BETWEEN TX AND RX,
//	AND SO PREVENTS LOOPBACK OR FULLDUP OPERATION

	BRG=(CLOCKFREQ/PORTVEC->PORTCONTROL.BAUDRATE)-2;

	SIOCW(12);			// Select WR12
	SIOCW(BRG & 0xff);	// SET LSB
	SIOCW(13);			// Select WR13
	SIOCW(BRG >> 8);	// SET MSB

	SIOCW(11);			// WR11
	SIOCW(0x70);		// RXC=DPLL, TXC=BRG

	PORTVEC->RXBRG = BRG;

//	CALC TX RATE

	PORTVEC->TXBRG = ((BRG+2)/32)-2;

	SIOCW(12);			// Select WR12
	SIOCW(BRG & 0xff);	// SET LSB
	SIOCW(13);			// Select WR13
	SIOCW(BRG >> 8);	// SET MSB

//	IF 7910/3105 PORTS, SET TXC=BRG, RXC=DPLL

//	IT SEEMS THE 3RD PORT IS MORE LIKELY TO BE USED WITH A SIMPLE
//	MODEM WITHOUT CLOCK GERERATION (EG BAYCOM MODEM), SO SET ALL 
//	PORTS THE SAME

	SIOCW(11);			// WR11
	SIOCW(0x70);		// RXC=DPLL, TXC=BRG
}

BOOLEAN INITREST(PHDLCDATA PORTVEC, PHDLCDATA PrevPort)
{
	BPQHDLC_ADDCHANNEL_INPUT AddParams;
	VOID * Return = NULL;
	int cb;

/*	mov     IRQHand[EBX],0	; in case already hooked by another port

	MOV	PORTINTERRUPT[EBX],OFFSET32 SIOINT

	CMP	EDI,0
	JNE SHORT INTDONE		; ALREADY SET UP

	CALL	HOOKINT			; INTERRUPT

INTDONE:

	CALL	RXAINIT
;
*/


//	Pass Params to the driver

	AddParams.IOBASE = PORTVEC->PORTCONTROL.IOBASE;
	AddParams.IOLEN = PORTVEC->IOLEN;
	AddParams.Interrupt = PORTVEC->PORTCONTROL.INTLEVEL;
	AddParams.ASIOC = PORTVEC->ASIOC;
	AddParams.BSIOC = PORTVEC->BSIOC;
	AddParams.SIOC = PORTVEC->SIOC;
	AddParams.SIO = PORTVEC->SIO;
	AddParams.TXBRG = PORTVEC->TXBRG;
	AddParams.RXBRG = PORTVEC->RXBRG;
	AddParams.WR10 = PORTVEC->WR10;
	AddParams.Channel = PORTVEC->PORTCONTROL.CHANNELNUM;
	AddParams.SOFTDCDFLAG = PORTVEC->PORTCONTROL.SOFTDCDFLAG;
	AddParams.TXDELAY = PORTVEC->PORTCONTROL.PORTTXDELAY;
	AddParams.PERSISTANCE = PORTVEC->PORTCONTROL.PORTPERSISTANCE;
	if (PrevPort)
		AddParams.OtherChannel = PrevPort->DRIVERPORTTABLE;
	else
		AddParams.OtherChannel = 0;

	fResult = DeviceIoControl(
			hDevice,									// device handle
			IOCTL_BPQHDLC_ADDCHANNEL,					// control code
			&AddParams,sizeof(BPQHDLC_ADDCHANNEL_INPUT),	// input parameters
	        &Return,4,&cb,								// output parameters
			0);

	PORTVEC->DRIVERPORTTABLE = Return;

	if (Return == NULL)
	{
		// Init Failed, probably because resources were not alllocated to driver

		WritetoConsole("Kernel Driver Init Failed - Check Resource Allocation");
		return (FALSE);
	}

	PORTVEC->RR0 = SIOCR;	//GET INITIAL RR0


	return TRUE;
}


int INITPORT(PHDLCDATA PORTVEC)
{
 	PHDLCDATA PreviousPort;

	//	SEE IF C OR D. If so, Adjust IOBASE and change to A/B

	switch (PORTVEC->PORTCONTROL.PORTTYPE)
	{
		case 10:				// RLC100
		case 12:				// RLC400
		case 20:				// PA0HZP OPTO-SCC
			
			CHECKCHAN(PORTVEC, 4);			// Channels are 4 apart
			break;

		case 18:				// Baycom
			
			CHECKCHAN(PORTVEC, 2);			//Channels are 2 apart
			break;
	}

	//	By now Channel Should be only A or B

	if ((PORTVEC->PORTCONTROL.CHANNELNUM != 'A') && (PORTVEC->PORTCONTROL.CHANNELNUM != 'B'))
	{
			WritetoConsole("Invalid Channel\n");
			return FALSE;
	}

	PreviousPort = See_if_First_On_Card(PORTVEC);

	if (PreviousPort == (PHDLCDATA)-1)
	{
		// Two ports on same card have same Channel

			WritetoConsole("Duplicate Channels\n");
			return FALSE;
	}

	switch (PORTVEC->PORTCONTROL.PORTTYPE)
	{
	case 2:			// PC120
		
		PORTVEC->IOLEN = 8;					// I think! - Modem is at offfset 0, SCC at 4
		INITPART2(PORTVEC, 4, PreviousPort);	// SCC ADDRESS 4 Above Base Address
		if (PreviousPort == NULL) INITMODEM(PORTVEC);
		INITREST(PORTVEC, PreviousPort);

		return 0;

	case 4:			// DRSIINIT
		
		PORTVEC->IOLEN = 8;					// SCC at 0, CIO at 7
		if (PreviousPort == NULL) INITCIO(PORTVEC);	// SET UP CIO FOR /32 UNLESS Already Done
		INITPART2(PORTVEC, 0, PreviousPort);
		if (PORTVEC->PORTCONTROL.BAUDRATE) STARTCIO(PORTVEC);
		INITREST(PORTVEC, PreviousPort);

		return TRUE;
	
	case 6:

		WritetoConsole("TYPE=TOSH Not Supported\n");
		return FALSE;

	case 10:				// RLC100
	case 12:				// RLC400

		PORTVEC->IOLEN = 4;						// 2 * SCC, but each SCC can be on it's own
		INITPART2(PORTVEC, 0, PreviousPort);
		INITREST(PORTVEC, PreviousPort);

		return TRUE;

	case 18:				// Baycom

		PORTVEC->IOLEN = 8;						// 2 * SCC, but Need at least 6 Addresses
		BINITPART2(PORTVEC, 0, PreviousPort);
		INITREST(PORTVEC, PreviousPort);

		return 0;
	
	case 20:				// PA0HZP OPTO-SCC

		PORTVEC->IOLEN = 4;						// 2 * SCC, but each SCC can be on it's own
		INITPART2(PORTVEC, 0, PreviousPort);
		INITREST(PORTVEC, PreviousPort);
		return TRUE;
	}

	return FALSE;
}
