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


//	TNC Emulator Module for BPQ32 switch

// Supports TNC2 and WA8DED Hostmode interfaces


#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include "CHeaders.h"

#define LF 10
#define CR 13

#define Connect(stream) SessionControl(stream,1,0)
#define Disconnect(stream) SessionControl(stream,2,0)
#define ReturntoNode(stream) SessionControl(stream,3,0)
#define ConnectUsingAppl(stream, appl) SessionControl(stream, 0, appl)

int APIENTRY SendMsg(int stream, char * msg, int len);

VOID SENDPACKET(struct TNCDATA * TNC);
VOID CHECKCTS(struct TNCDATA * TNC);
VOID SETCOMMANDMODE(struct TNCDATA * TNC);
VOID SETCOMM00(struct TNCDATA * TNC);
VOID SENDREPLY(struct TNCDATA * TNC, char * Msg, int Len);
VOID TNCCOMMAND(struct TNCDATA * TNC);
VOID TNC2PutChar(struct TNCDATA * TNC, int Char);
VOID KBECHO(struct TNCDATA * TNC, int Char);
VOID KBNORM(struct TNCDATA * TNC, int Char);
VOID PUTCHARINBUFFER(struct TNCDATA * TNC, int Char);
VOID TNC2GetChar(struct TNCDATA * TNC, int * returnedchar, int * moretocome);
VOID CONNECTTONODE(struct TNCDATA * TNC);
DllImport int APIENTRY ChangeSessionCallsign(int Stream, unsigned char * AXCall);
DllImport int APIENTRY GetCallsign(int stream, char * callsign);
VOID GETDATA(struct TNCDATA * TNC);
VOID DOCONMODECHANGE(struct TNCDATA * TNC);
VOID SEND_CONNECTED(struct TNCDATA * TNC);
VOID READCHANGE(int Stream);
VOID DOMONITORING(int NeedTrace);
int APIENTRY DecodeFrame(MESSAGE * msg, char * buffer, int Stamp);
time_t APIENTRY GetRaw(int stream, char * msg, int * len, int * count);
BOOL TfPut(struct TNCDATA * TNC, UCHAR character);
int IntDecodeFrame(MESSAGE * msg, char * buffer, int Stamp, UINT Mask, BOOL APRS, BOOL MCTL);
int DATAPOLL(struct TNCDATA * TNC, struct StreamInfo * Channel);
int STATUSPOLL(struct TNCDATA * TNC, struct StreamInfo * Channel);
int PROCESSHOSTPACKET(struct StreamInfo * Channel, struct TNCDATA * TNC);
VOID ProcessKPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Len);
VOID ProcessPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Len);;
int KANTConnected(struct TNCDATA * conn, struct StreamInfo * channel, int Stream);
int KANTDisconnected(struct TNCDATA * conn, struct StreamInfo * channel, int Stream);
VOID SendKISSData(struct TNCDATA * conn, UCHAR * txbuffer, int Len);
VOID ProcessSCSPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Length);
VOID TNCPoll();
unsigned long _beginthread( void( *start_address )(), unsigned stack_size, void * arglist);
VOID DisableAppl(struct TNCDATA * TNC);
int BPQSerialSetPollDelay(HANDLE hDevice, int PollDelay);



#define TNCBUFFLEN 1024

extern struct TNCDATA * TNCCONFIGTABLE;

struct TNCDATA * TNC2TABLE;		// malloc'ed
extern int NUMBEROFTNCPORTS;

struct TNCDATA TDP;			// Only way I can think of to get offets to port data into cmd table

//	MODEFLAG DEFINITIONS

#define COMMAND	1
#define TRANS 2
#define CONV 4

//	APPLFLAGS BITS

//CMD_TO_APPL	EQU	1B		; PASS COMMAND TO APPLICATION
//MSG_TO_USER	EQU	10B		; SEND "CONNECTED" TO USER
//MSG_TO_APPL	EQU	100B		; SEND "CONECTED" TO APPL

extern char pgm[256];	

int CloseDelay = 10;	// Close after connect fail delay

MESSAGE MONITORDATA;		// RAW FRAME FROM NODE

char NEWCALL[11];
	
//TABLELEN	DW	TYPE TNCDATA

char LNKSTATEMSG[] = "Link state is: ";
char CONNECTEDMSG[] = "CONNECTED to ";
char WHATMSG[] = "Eh?\rcmd:";
char CMDMSG[] =	"cmd:";


char DISCONNMSG[] = "\r*** DISCONNECTED\r";

char CONMSG1[] = "\r*** CONNECTED to ";
char CONCALL[10];


char SIGNON[] = "\r\rG8BPQ TNC2 EMULATOR\r\r";

char CONMSG[] ="\r*** CONNECTED to ";
char SWITCH[] = "SWITCH\r";
char SWITCHSP[]	= "SWITCH    ";

char WAS[] = " was ";
char VIA[] = " via ";
char OFF[] = "OFF\r";
char ON[] = "ON \r";

// BPQ Serial Device Support

// On W2K and above, BPQVIrtualCOM.sys provides a pair of cross-connected devices, and a control channel
//	to enumerate, add and delete devices.

// On Win98 BPQVCOMM.VXD provides a single IOCTL interface, over which calls for each COM device are multiplexed

#ifdef WIN32

#define IOCTL_SERIAL_SET_BAUD_RATE      CTL_CODE(FILE_DEVICE_SERIAL_PORT, 1,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_QUEUE_SIZE     CTL_CODE(FILE_DEVICE_SERIAL_PORT, 2,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_LINE_CONTROL   CTL_CODE(FILE_DEVICE_SERIAL_PORT, 3,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_BREAK_ON       CTL_CODE(FILE_DEVICE_SERIAL_PORT, 4,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_BREAK_OFF      CTL_CODE(FILE_DEVICE_SERIAL_PORT, 5,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_IMMEDIATE_CHAR     CTL_CODE(FILE_DEVICE_SERIAL_PORT, 6,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_TIMEOUTS       CTL_CODE(FILE_DEVICE_SERIAL_PORT, 7,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_TIMEOUTS       CTL_CODE(FILE_DEVICE_SERIAL_PORT, 8,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_DTR            CTL_CODE(FILE_DEVICE_SERIAL_PORT, 9,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_CLR_DTR            CTL_CODE(FILE_DEVICE_SERIAL_PORT,10,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_RESET_DEVICE       CTL_CODE(FILE_DEVICE_SERIAL_PORT,11,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_RTS            CTL_CODE(FILE_DEVICE_SERIAL_PORT,12,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_CLR_RTS            CTL_CODE(FILE_DEVICE_SERIAL_PORT,13,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_XOFF           CTL_CODE(FILE_DEVICE_SERIAL_PORT,14,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_XON            CTL_CODE(FILE_DEVICE_SERIAL_PORT,15,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_WAIT_MASK      CTL_CODE(FILE_DEVICE_SERIAL_PORT,16,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_WAIT_MASK      CTL_CODE(FILE_DEVICE_SERIAL_PORT,17,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_WAIT_ON_MASK       CTL_CODE(FILE_DEVICE_SERIAL_PORT,18,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_PURGE              CTL_CODE(FILE_DEVICE_SERIAL_PORT,19,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_BAUD_RATE      CTL_CODE(FILE_DEVICE_SERIAL_PORT,20,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_LINE_CONTROL   CTL_CODE(FILE_DEVICE_SERIAL_PORT,21,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_CHARS          CTL_CODE(FILE_DEVICE_SERIAL_PORT,22,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_CHARS          CTL_CODE(FILE_DEVICE_SERIAL_PORT,23,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_HANDFLOW       CTL_CODE(FILE_DEVICE_SERIAL_PORT,24,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_HANDFLOW       CTL_CODE(FILE_DEVICE_SERIAL_PORT,25,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_MODEMSTATUS    CTL_CODE(FILE_DEVICE_SERIAL_PORT,26,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_COMMSTATUS     CTL_CODE(FILE_DEVICE_SERIAL_PORT,27,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_XOFF_COUNTER       CTL_CODE(FILE_DEVICE_SERIAL_PORT,28,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_PROPERTIES     CTL_CODE(FILE_DEVICE_SERIAL_PORT,29,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_DTRRTS         CTL_CODE(FILE_DEVICE_SERIAL_PORT,30,METHOD_BUFFERED,FILE_ANY_ACCESS)


#define IOCTL_SERIAL_IS_COM_OPEN CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GETDATA     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x801,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SETDATA     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x802,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_SERIAL_SET_CTS     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x803,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_DSR     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x804,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_DCD     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x805,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_SERIAL_CLR_CTS     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x806,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_CLR_DSR     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x807,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_CLR_DCD     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x808,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_BPQ_ADD_DEVICE     CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x809,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQ_DELETE_DEVICE  CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x80a,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_BPQ_LIST_DEVICES   CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x80b,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define	IOCTL_BPQ_SET_POLLDELAY	 CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x80c,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define	IOCTL_BPQ_SET_DEBUGMASK	 CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x80d,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define W98_SERIAL_IS_COM_OPEN 0x800
#define W98_SERIAL_GETDATA     0x801
#define W98_SERIAL_SETDATA     0x802

#define W98_SERIAL_SET_CTS     0x803
#define W98_SERIAL_SET_DSR     0x804
#define W98_SERIAL_SET_DCD     0x805

#define W98_SERIAL_CLR_CTS     0x806
#define W98_SERIAL_CLR_DSR     0x807
#define W98_SERIAL_CLR_DCD     0x808

#define W98_BPQ_ADD_DEVICE     0x809
#define W98_BPQ_DELETE_DEVICE  0x80a
#define W98_BPQ_LIST_DEVICES   0x80b

#define	W98_BPQ_SET_POLLDELAY	 0x80c
#define	W98_BPQ_SET_DEBUGMASK	 0x80d

#define W98_SERIAL_GET_COMMSTATUS    27
#define W98_SERIAL_GET_DTRRTS        30

#define DebugModemStatus 1
#define DebugCOMStatus 2
#define DebugWaitCompletion 4
#define DebugReadCompletion 8


HANDLE hControl;

BOOL Win98;

typedef struct _SERIAL_STATUS {
    ULONG Errors;
    ULONG HoldReasons;
    ULONG AmountInInQueue;
    ULONG AmountInOutQueue;
    BOOL EofReceived;
    BOOL WaitForImmediate;
} SERIAL_STATUS,*PSERIAL_STATUS;

#endif

#ifndef WIN32
//	#include <pty.h>

HANDLE LinuxOpenPTY(char * Name)
{            
	// Open a Virtual COM Port

	HANDLE hDevice, slave;;
	char slavedevice[80];
	int ret;
	u_long param=1;
	struct termios term;

#ifdef MACBPQ

	// Create a pty pair
	
	openpty(&hDevice, &slave, &slavedevice[0], NULL, NULL);
	close(slave);

#else
	 
	hDevice = posix_openpt(O_RDWR|O_NOCTTY);

	if (hDevice == -1 || grantpt (hDevice) == -1 || unlockpt (hDevice) == -1 ||
		 ptsname_r(hDevice, slavedevice, 80) != 0)
	{
		perror("Create PTY pair failed");
		return -1;
	} 

#endif

	printf("slave device: %s. ", slavedevice);
 
	if (tcgetattr(hDevice, &term) == -1)
	{
		perror("tty_speed: tcgetattr");
		return FALSE;
	}

	cfmakeraw(&term);

	if (tcsetattr(hDevice, TCSANOW, &term) == -1)
	{
		perror("tcsetattr");
		return -1;
	}

	ioctl(hDevice, FIONBIO, &param);

	chmod(slavedevice, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP|S_IROTH|S_IWOTH);

	unlink (Name);
		
	ret = symlink (slavedevice, Name);
		
	if (ret == 0)
		printf ("symlink to %s created\n", Name);
	else
		printf ("symlink to %s failed\n", Name);	
	
	return hDevice;
}
#else

HANDLE BPQOpenSerialPort(struct TNCDATA * TNC, DWORD * lasterror)
{
	// Open a Virtual COM Port

	int port = TNC->ComPort;
	char szPort[80];
	HANDLE hDevice;
	int Err;

	*lasterror=0;

	if (Win98)
	{
		sprintf( szPort, "\\\\.\\COM%d",port) ;

		hDevice = CreateFile( szPort, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );
				  
		if (hDevice == (HANDLE) -1 )
		{
			// If In Use(5) ok, else fail

			if (GetLastError() == 5)
				return (HANDLE)(port<<16);			// Port Number is a pseudohandle to the device

			return (HANDLE) -1;
		}

		CloseHandle(hDevice);

		return (HANDLE)(port<<16);			// Port Number is a pseudohandle to the device
	}

	// Try New Style VCOM firsrt

	sprintf( szPort, "\\\\.\\pipe\\BPQCOM%d", port ) ;

	hDevice = CreateFile( szPort, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

	Err = GetLastError();

	if (hDevice != (HANDLE) -1)
	{
			TNC->NewVCOM = TRUE;
			TNC->PortEnabled = TRUE;
			Err = GetFileType(hDevice);
	}
	else
	{

		// Try old style 	

		sprintf(szPort, "\\\\.\\BPQ%d", port ) ;
   
	
		hDevice = CreateFile( szPort, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

		if (TNC->PollDelay)
			BPQSerialSetPollDelay(hDevice, TNC->PollDelay);

	}		  
	if (hDevice == (HANDLE) -1 )
	{
		*lasterror=GetLastError();
	}

   return hDevice;
}
#endif

int BPQSerialSetCTS(HANDLE hDevice)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl,(UINT)(UINT)hDevice | W98_SERIAL_SET_CTS,NULL,0,NULL,0, &bytesReturned,NULL);
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_SET_CTS,NULL,0,NULL,0, &bytesReturned,NULL);

#endif
}

int BPQSerialSetDSR(HANDLE hDevice)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_SET_DSR,NULL,0,NULL,0, &bytesReturned,NULL);
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_SET_DSR, NULL,0,NULL,0, &bytesReturned,NULL);
#endif
}

int BPQSerialSetDCD(HANDLE hDevice)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_SET_DCD,NULL,0,NULL,0, &bytesReturned,NULL);
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_SET_DCD,NULL,0,NULL,0, &bytesReturned,NULL);
#endif
}

int BPQSerialClrCTS(HANDLE hDevice)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_CLR_CTS,NULL,0,NULL,0, &bytesReturned,NULL);
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_CLR_CTS,NULL,0,NULL,0, &bytesReturned,NULL);
#endif                  
}
int BPQSerialClrDSR(HANDLE hDevice)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_CLR_DSR,NULL,0,NULL,0, &bytesReturned,NULL);
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_CLR_DSR,NULL,0,NULL,0, &bytesReturned,NULL);            
#endif
}

int BPQSerialClrDCD(HANDLE hDevice)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_CLR_DCD,NULL,0,NULL,0, &bytesReturned,NULL);
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_CLR_DCD, NULL,0,NULL,0, &bytesReturned,NULL);
#endif                     
}

int BPQSerialSendData(struct TNCDATA * TNC, UCHAR * Message,int MsgLen)
{
	HANDLE hDevice = TNC->hDevice;
	ULONG bytesReturned;

	// Host Mode code calls BPQSerialSendData for all ports, so it a real port, pass on to real send routine

	if (!TNC->VCOM)
		return WriteCOMBlock(TNC->hDevice, Message, MsgLen);

#ifndef WIN32
	
	// Linux usies normal IO for all ports
	return WriteCOMBlock(TNC->hDevice, Message, MsgLen);

#else

	if (MsgLen > 4096 )	return ERROR_INVALID_PARAMETER;
	
	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_SETDATA,Message,MsgLen,NULL,0, &bytesReturned,NULL);
	else
	{
		if (TNC->NewVCOM)
		{
			// Have to escape all oxff chars, as these are used to get status info 

			UCHAR NewMessage[1000];
			UCHAR * ptr1 = Message;
			UCHAR * ptr2 = NewMessage;
			UCHAR c;

			int Length = MsgLen;

			while (Length != 0)
			{
				c = *(ptr1++);
				*(ptr2++) = c;

				if (c == 0xff)
				{
					*(ptr2++) = c;
					MsgLen++;
				}
				Length--;
			}

			return WriteFile(hDevice, NewMessage, MsgLen, &bytesReturned, NULL);
		}
		else
			return DeviceIoControl(hDevice,IOCTL_SERIAL_SETDATA,Message,MsgLen,NULL,0, &bytesReturned,NULL);
	}      
#endif
}

int BPQSerialGetData(struct TNCDATA * TNC, UCHAR * Message, unsigned int BufLen, ULONG * MsgLen)
{
#ifdef WIN32
	DWORD dwLength = 0;
	DWORD Available = 0;
	HANDLE hDevice = TNC->hDevice;
	int Length, RealLen = 0;

	if (BufLen > 4096 )	return ERROR_INVALID_PARAMETER;
	
	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_GETDATA,NULL,0,Message,BufLen,MsgLen,NULL);

	if (TNC->NewVCOM)
	{
		int ret = PeekNamedPipe(hDevice, NULL, 0, NULL, &Available, NULL);

		if (ret == 0)
		{
			ret = GetLastError();

			if (ret == ERROR_BROKEN_PIPE)
			{
				CloseHandle(hDevice);
				hDevice = INVALID_HANDLE_VALUE;
				return 0;
			}
		}

		if (Available > BufLen)
			Available = BufLen;
		
		if (Available)
		{
			UCHAR * ptr1 = Message;
			UCHAR * ptr2 = Message;
			UCHAR c;
			
			ReadFile(hDevice, Message, Available, &Length, NULL);

			// Have to look for FF escape chars

			RealLen = Length;

			while (Length != 0)
			{
				c = *(ptr1++);
				Length--;

				if (c == 0xff)
				{
					c = c = *(ptr1++);
					Length--;
					
					if (c == 0xff)			// ff ff means ff
					{
						RealLen--;
					}
					else
					{
						// This is connection statua from other end

						RealLen -= 2;
//						TNC->PortEnabled = c;
						continue;
					}
				}
				*(ptr2++) = c;
			}
		}
		*MsgLen = RealLen;
		return 0;
	}

	return DeviceIoControl(hDevice,IOCTL_SERIAL_GETDATA,NULL,0,Message,BufLen,MsgLen,NULL); 
}
#else
	return 0;
}
#endif

int BPQSerialGetQCounts(HANDLE hDevice,ULONG * RXCount, ULONG * TXCount)
{
#ifndef WIN32
	return 0;
#else

	SERIAL_STATUS Resp;
	int MsgLen;
	int ret;

	if (Win98)
		ret = DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_GET_COMMSTATUS,NULL,0,&Resp,sizeof(SERIAL_STATUS),&MsgLen,NULL);
	else
		ret = DeviceIoControl(hDevice,IOCTL_SERIAL_GET_COMMSTATUS,NULL,0,&Resp,sizeof(SERIAL_STATUS),&MsgLen,NULL);

    *RXCount=Resp.AmountInInQueue;
	*TXCount=Resp.AmountInOutQueue;

	return ret;
#endif
}

int BPQSerialGetDeviceList(HANDLE hDevice,ULONG * Slot,ULONG * Port)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	return  DeviceIoControl (hDevice,IOCTL_BPQ_LIST_DEVICES,Slot,4,Port,4,&bytesReturned,NULL);
#endif
}

int BPQSerialIsCOMOpen(HANDLE hDevice,ULONG * Count)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_IS_COM_OPEN,NULL,0,Count,4,&bytesReturned,NULL);                
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_IS_COM_OPEN,NULL,0,Count,4,&bytesReturned,NULL);                
#endif
}

int BPQSerialGetDTRRTS(HANDLE hDevice, ULONG * Flags)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;

	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_SERIAL_GET_DTRRTS,NULL,0,Flags,4,&bytesReturned,NULL);                
	else
		return DeviceIoControl(hDevice,IOCTL_SERIAL_GET_DTRRTS,NULL,0,Flags,4,&bytesReturned,NULL);                
#endif
}

int BPQSerialSetPollDelay(HANDLE hDevice, int PollDelay)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;
	
	if (Win98)
		return DeviceIoControl(hControl, (UINT)hDevice | W98_BPQ_SET_POLLDELAY,&PollDelay,4,NULL,0, &bytesReturned,NULL);
	else
		return DeviceIoControl(hDevice,IOCTL_BPQ_SET_POLLDELAY,&PollDelay,4,NULL,0, &bytesReturned,NULL);

#endif                
}

int BPQSerialSetDebugMask(HANDLE hDevice, int DebugMask)
{
#ifndef WIN32
	return 0;
#else

	ULONG bytesReturned;
	
	return DeviceIoControl(hDevice, IOCTL_BPQ_SET_DEBUGMASK, &DebugMask, 4, NULL, 0, &bytesReturned,NULL);
#endif                  
}

int LocalSessionState(int stream, int * state, int * change, BOOL ACK)
{
	//	Get current Session State. Any state changed is ACK'ed
	//	automatically. See BPQHOST functions 4 and 5.

	// Local version without semaphore or checktimer

	BPQVECSTRUC * HOST = &BPQHOSTVECTOR[stream -1];		// API counts from 1

	//	CX = 0 if stream disconnected or CX = 1 if stream connected
	//	DX = 0 if no change of state since last read, or DX = 1 if
	//	       the connected/disconnected state has changed since
	//	       last read (ie. delta-stream status).

	//	HOSTFLAGS = Bit 80 = Allocated
	//		  Bit 40 = Disc Request
	//		  Bit 20 = Stay Flag
	//		  Bit 02 and 01 State Change Bits

	if ((HOST->HOSTFLAGS & 3) == 0)		
		// No Chaange
		*change = 0;
	else
		*change = 1;

	if (HOST->HOSTSESSION)			// LOCAL SESSION
		// Connected
		*state = 1;
	else
		*state = 0;
	
	if (ACK)
		HOST->HOSTFLAGS &= 0xFC;		// Clear Change Bits		

	return 0;
}




VOID ONOFF(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	//	PROCESS COMMANDS WITH ON/OFF PARAM

	char Param = *Tail;
	UINT offset, DPBASE;
	UCHAR * valueptr;
	UCHAR oldvalue, newvalue = 0xff;

	char Response[80];
	int len;

	valueptr = (UCHAR *)TNC;
	offset = (UINT)CMD->CMDFLAG;
	DPBASE = (UINT)&TDP;
	offset -= DPBASE;
	valueptr += offset;
	oldvalue = (UCHAR)*valueptr;


	switch(Param)
	{
	case ' ':
		break;
	case 'Y':
		newvalue = 1;
		break;
	case 'N':
		newvalue = 0;
		break;
	case 'O':
		if (Tail[1] == 'N')
			newvalue = 1;
		else
			newvalue = 0;
		break;
	}

	if (newvalue == 255)
	{
		len = sprintf(Response, "%s %s\r", CMD->String, (oldvalue)?"ON":"OFF");
	}
	else
	{
		len = sprintf(Response, "%s was %s\r", CMD->String, (oldvalue)?"ON":"OFF");
		*valueptr = newvalue;
	}
	SENDREPLY(TNC, Response, len);
}



VOID ONOFF_CONOK(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	ONOFF(TNC, Tail, CMD);

	//	UPDATE APPL FLAGS ON NODE PORT

	if (TNC->CONOK)
		SetAppl(TNC->BPQPort, TNC->APPLFLAGS, TNC->APPLICATION);
	else
		SetAppl(TNC->BPQPort, TNC->APPLFLAGS, 0);
}

VOID SETMYCALL(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	char Response[80];
	int len;
	char Call[10] = "         ";

	if (*Tail == ' ')
	{
		// REQUEST FOR CURRENT STATE

		len = sprintf(Response, "MYCALL %s\r", TNC->MYCALL);
	}
	else
	{
		strlop(Tail,' ');;
		memcpy(Call, Tail, strlen(Tail) + 1);
		len = sprintf(Response, "MYCALL was %s\r", TNC->MYCALL);
		memcpy(TNC->MYCALL, Call, 10);
	}

	SENDREPLY(TNC, Response, len);
}
VOID BTEXT(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
}
VOID VALUE(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	//	PROCESS COMMANDS WITH decimal value

	char Param = *Tail;
	UINT offset, DPBASE;
	UCHAR * valueptr;
	int oldvalue, newvalue;

	char Response[80];
	int len;

	valueptr = (UCHAR *)TNC;
	offset = (UINT)CMD->CMDFLAG;
	DPBASE = (UINT)&TDP;
	offset -= DPBASE;
	valueptr += offset;
	oldvalue = *valueptr;

	strlop(Tail, ' ');
	
	if (Tail[0])
	{
		newvalue = atoi(Tail);
		len = sprintf(Response, "%s was %d\r", CMD->String, oldvalue);
		*valueptr = newvalue;
	}
	else
	{
		len = sprintf(Response, "%s %d\r", CMD->String, oldvalue);
	}
	SENDREPLY(TNC, Response, len);
}

VOID VALHEX(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	//	PROCESS COMMANDS WITH decimal value

	char Param = *Tail;
	UINT offset, DPBASE;

	UCHAR * valueptr;
	UINT * intvalueptr;
	UINT oldvalue, newvalue;

	char Response[80];
	int len;

	valueptr = (UCHAR *)TNC;
	offset = (UINT)CMD->CMDFLAG;
	DPBASE = (UINT)&TDP;
	offset -= DPBASE;
	valueptr += offset;
	intvalueptr = (UINT *)valueptr;
	oldvalue = *intvalueptr;

	strlop(Tail, ' ');

	if (Tail[0])
	{
		newvalue = (UINT)strtol(Tail, NULL, 16);
		len = sprintf(Response, "%s was $%x\r", CMD->String, oldvalue);
		*intvalueptr = newvalue;
	}
	else
	{
		len = sprintf(Response, "%s $%x\r", CMD->String, oldvalue);
	}
	SENDREPLY(TNC, Response, len);
}

VOID APPL_VALHEX(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	int ApplNum = 1;
	UINT APPLMASK;

	VALHEX(TNC, Tail, CMD);

	//	UPDATE APPL FLAGS ON NODE PORT

	if (TNC->CONOK)
		SetAppl(TNC->BPQPort, TNC->APPLFLAGS, TNC->APPLICATION);
	else
		SetAppl(TNC->BPQPort, TNC->APPLFLAGS, 0);

	// Set MYCALL to APPLCALL

	APPLMASK = TNC->APPLICATION;
	ApplNum = 1;

	while  (APPLMASK && (APPLMASK & 1) == 0)
	{
		ApplNum++;
		APPLMASK >>= 1;
	}

	if (TNC->CONOK && TNC->APPLICATION)
		memcpy(TNC->MYCALL, GetApplCall(ApplNum), 10);

}
VOID CSWITCH(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	char Response[80];
	int len;

	len = sprintf(Response, "%s", CMDMSG);
	SENDREPLY(TNC, Response, len);

	CONNECTTONODE(TNC);

}
VOID CONMODE(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	SENDREPLY(TNC, CMDMSG, 4);
}

VOID TNCCONV(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	TNC->MODEFLAG |= CONV;
	TNC->MODEFLAG &= ~(COMMAND+TRANS);
}

VOID TNCNODE(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	//	CONNECT TO NODE

	TNC->VMSR |= 0x88;		// SET CONNECTED

	TNC->MODEFLAG |= CONV;		// INTO CONVERSE MODE
	TNC->MODEFLAG &= ~(COMMAND+TRANS);

	CONNECTTONODE(TNC);
	READCHANGE(TNC->BPQPort);		//CLEAR STATUS CHANGE (TO AVOID SUPURIOUS "CONNECTED TO")
}


VOID TNCCONNECT(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	char Response[80];
	int len;
	char Call[10] = "";

	if (*Tail == ' ')
	{
		// REQUEST FOR CURRENT STATE

		len = sprintf(Response, "%s", LNKSTATEMSG);

		if (TNC->VMSR & 0x80)
		{
			GetCallsign(TNC->BPQPort, Call);
			strlop(Call, ' ');

			len = sprintf(Response, "%s%s%s\r", LNKSTATEMSG, CONNECTEDMSG, Call);
		}
		else
		{
			len = sprintf(Response, "%s%s", LNKSTATEMSG, DISCONNMSG+5);
		}

		SENDREPLY(TNC, Response, len);
		return;
	}

	//	CONNECT, BUT NOT TO SWITCH - CONNECT TO NODE, THEN PASS TO IT FOR PROCESSING

	TNCNODE(TNC, Tail, CMD);
	strcat(TNC->TXBUFFER, "\r");
	TNC->MSGLEN = strlen(TNC->TXBUFFER);

	SENDPACKET(TNC);		// Will now go to node

}
VOID TNCDISC(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	Disconnect(TNC->BPQPort);

	SENDREPLY(TNC, CMDMSG, 4);
}

VOID READCHANGE(int Stream)
{
	int dummy;
	LocalSessionState(Stream, &dummy, &dummy, TRUE);
}

VOID TNCRELEASE(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	ReturntoNode(TNC->BPQPort);

	TNC->VMSR &= 0x7F;			// DROP DCD
	TNC->VMSR |= 8;				//DELTA DCD

	SENDREPLY(TNC, CMDMSG, 4);
}
VOID TNCTRANS(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
	//	MAKE PRETTY SURE THIS ISNT A BIT OF STRAY DATA

 	if (TNC->MSGLEN > 6)
		return;

	TNC->MODEFLAG |= TRANS;
	TNC->MODEFLAG &= ~(COMMAND+CONV);
}
static VOID RESTART(struct TNCDATA * TNC)
{
	//	REINITIALISE CHANNEL

	TNC->PUTPTR = TNC->GETPTR = &TNC->RXBUFFER[0];
	TNC->RXCOUNT = 0;
		
	TNC->VLSR = 0x20;
	TNC->VMSR = 0x30;

	TNC->MODEFLAG = COMMAND;
	TNC->SENDPAC = 13;
	TNC->CRFLAG = 1;
	TNC->MALL = 1;
	TNC->MMASK = -1;			//  MONITOR MASK FOR PORTS
	TNC->TPACLEN = PACLEN;		// TNC PACLEN

	TNC->COMCHAR = 3;
	TNC->CMDTIME = 10;			// SYSTEM TIMER = 100MS
	TNC->CURSOR = &TNC->TXBUFFER[0]; // RESET MESSAGE START
	TNC->MSGLEN = 0;

	SENDREPLY(TNC, SIGNON, 23);
}


static VOID UNPROTOCMD(struct TNCDATA * TNC, char * Tail, CMDX * CMD)
{
}




CMDX COMMANDLIST[] =
{
	"AUTOLF   ",2, ONOFF, &TDP.AUTOLF,
	"BBSMON   ",6, ONOFF, &TDP.BBSMON,
	"BTEXT",2,BTEXT,0,
	"CONOK   ",4,ONOFF_CONOK,&TDP.CONOK,
	"C SWITCH",8,CSWITCH,0,
	"CBELL   ",2,ONOFF,&TDP.CBELL,
	"CMDTIME ",2,VALHEX,&TDP.CMDTIME,
	"COMMAND ",3,VALHEX,&TDP.COMCHAR,
	"CONMODE ",4,CONMODE,0,
	"CPACTIME",2,ONOFF,&TDP.CPACTIME,
	"CR      ",2,ONOFF,&TDP.CRFLAG,
	"APPLFLAG",5,APPL_VALHEX,&TDP.APPLFLAGS,
	"APPL    ",4,APPL_VALHEX,&TDP.APPLICATION,
	"CONVERS ",4,TNCCONV,0,
	"CONNECT ",1,TNCCONNECT,0,
	"DISCONNE",1,TNCDISC,0,
	"ECHO    ",1,ONOFF,&TDP.ECHOFLAG,
	"FLOW    ",4,ONOFF,&TDP.FLOWFLAG,
	"HEADERLN",2,ONOFF,&TDP.HEADERLN,
	"K       ",1,TNCNODE,0,
	"MTXFORCE",4,ONOFF,&TDP.MTXFORCE,
	"LFIGNORE",3,ONOFF,&TDP.LFIGNORE,
	"MTX     ",3,ONOFF,&TDP.MTX,
	"MALL    ",2,ONOFF,&TDP.MALL,
	"MCOM    ",4,ONOFF,&TDP.MCOM,
	"MCON    ",2,ONOFF,&TDP.MCON,
	"MMASK   ",2,VALHEX,&TDP.MMASK,
	"MONITOR ",3,ONOFF,&TDP.TRACEFLAG,
	"MYCALL  ",2,SETMYCALL,0,
	"NEWMODE ",2,ONOFF,&TDP.NEWMODE,
	"NODE    ",3,TNCNODE,0,
	"NOMODE  ",2,ONOFF,&TDP.NOMODE,
	"SENDPAC ",2,VALHEX,&TDP.SENDPAC,
	"PACLEN  ",1,VALUE,&TDP.TPACLEN,
	"RELEASE ",3,TNCRELEASE,0,
	"RESTART ",7,RESTART,0,
	"TRANS   ",1,TNCTRANS,0,
	"UNPROTO ",1,UNPROTOCMD,0,
};

static CMDX * CMD = NULL;

static int NUMBEROFTNCCOMMANDS = sizeof(COMMANDLIST)/sizeof(CMDX);

/*NEWVALUE	DW	0
HEXFLAG		DB	0


NUMBER		DB	4 DUP (0),CR
NUMBERH		"$0000",CR

BADMSG		"?bad parameter",CR,0

BTHDDR	,0		; CHAIN
		DB	0		; PORT	
		DW	7		; LENGTH
		DB	0F0H		; PID
BTEXTFLD	DB	0DH,256 DUP (0)

CMDENDADDR,0		; POINTER TO END OF COMMAND

MBOPTIONBYTE	DB	0

NORMCALL	DB	10 DUP (0)
AX25CALL	DB	7 DUP (0)

CONNECTCALL	DB	10 DUP (0)	; CALLSIGN IN CONNECT MESSAGE
DIGICALL	DB	10 DUP (0)	; DIGI IN CONNECT COMMAND
AX25STRING	DB	64 DUP (0)	; DIGI STRING IN AX25 FORMAT
DIGIPTR	,0		; POINTER TO CURRENT DIGI IN STRING

NORMLEN	,0

	EVEN
*/

int TRANSDELAY = 10;		// 1 SEC

//UNPROTOCALL	DB	"UNPROTO",80 DUP (0)

char MONBUFFER[1000];

VOID TNC2GetChar(struct TNCDATA * TNC, int * returnedchar, int * moretocome)
{
	// Poll Node

	GETDATA(TNC);

	*returnedchar = -1;
	*moretocome = 0;

	if (TNC->RXCOUNT == 0)
		return;

	*returnedchar = *(TNC->GETPTR++);

	if (TNC->GETPTR == &TNC->RXBUFFER[TNCBUFFLEN])
		TNC->GETPTR = &TNC->RXBUFFER[0];
	
	*moretocome = --TNC->RXCOUNT;		//ANY MORE?

	if (TNC->RXCOUNT < 128)				// GETTING LOW?
	{
		if (TNC->RTSFLAG & 1)			//  RTS UP?
		{
			//	RTS HAD BEEN DROPPED TO STOP OTHER END SENDING - RAISE IT AGAIN
	
			TNC->RTSFLAG &= 0xFE;
		}
	}
}

BOOL TNC2GetVMSR(struct TNCDATA * TNC,int * returnedchar)
{            
	*returnedchar = TNC->VMSR;

	TNC->VMSR &= 0xF0;			// CLEAR DELTA BITS
	return TRUE;
}

BOOL TNCRUNNING;;

VOID TNCBGThread()
{
	TNCRUNNING = TRUE;

	while (TNCRUNNING)
	{
		TNCPoll();
		Sleep(10);
	}
}


VOID AllocateDEDChannel(struct TNCDATA * TNC, int Num)
{
	struct StreamInfo * Channel = zalloc(sizeof(struct StreamInfo));
	char * PNptr;

	// Only show last element of name on Streams display

	PNptr = &TNC->PORTNAME[0];

	while (strchr(PNptr, '/'))
		PNptr = strchr(PNptr, '/') + 1;

	sprintf(pgm, "DED %s", PNptr);

	TNC->Channels[Num] = Channel;
	Channel->BPQStream = FindFreeStream();
	READCHANGE(Channel->BPQStream);					// Prevent Initial *** disconnected
	Debugprintf("BPQ32 DED Stream %d  BPQ Stream %d", Num, Channel->BPQStream );

	if (TNC->MODE)									// if host mode, set appl
		SetAppl(Channel->BPQStream, TNC->APPLFLAGS, TNC->APPLICATION);

	strcpy(pgm, "bpq32.exe");
}


BOOL InitializeTNCEmulator()
{
	int resp, i;
	ULONG OpenCount = 0;
	DWORD Errorval;
	int ApplNum = 1;
	UINT APPLMASK;

	struct TNCDATA * TNC = TNCCONFIGTABLE;

	TNC2TABLE = TNCCONFIGTABLE;
	
	while (TNC)
	{
		// Com Port may be a hardware device (ie /dev/ttyUSB0) COMn or VCOMn (BPQ Virtual COM)

		char * Baud = strlop(TNC->PORTNAME, ',');
		char * PNptr;

		PNptr = &TNC->PORTNAME[0];

		// Only show last element of name on Streams display

		while (strchr(PNptr, '/'))
		{
			PNptr = strchr(PNptr, '/') + 1;
		}
		switch (TNC->Mode)
		{
		case TNC2:

			sprintf(pgm, "TNC2 %s", PNptr);

			TNC->BPQPort = FindFreeStream();

			if (TNC->BPQPort == 0)
			{
				Debugprintf("Insufficient free Streams for TNC2 Emulator");
				return FALSE;
			}
			
			READCHANGE(TNC->BPQPort);					// Prevent Initial *** disconnected

			strcpy(pgm, "bpq32.exe");
			TNC->MODEFLAG = COMMAND;
			TNC->SENDPAC = 13;
			TNC->CRFLAG = 1;
			TNC->MTX = 1;
			TNC->MCOM = 1;
			TNC->MMASK = -1;			//  MONITOR MASK FOR PORTS
			TNC->TPACLEN = PACLEN;		// TNC PACLEN

			TNC->COMCHAR = 3;
			TNC->CMDTIME = 10;			// SYSTEM TIMER = 100MS

			break;

		case DED:

			if (TNC->HOSTSTREAMS == 0)
				TNC->HOSTSTREAMS = 4;		// Default

			TNC->MALL = 1;
			TNC->MTX = 1;
			TNC->MCOM = 1;
			TNC->MMASK = -1;			//  MONITOR MASK FOR PORTS
			TNC->TPACLEN = PACLEN;		// TNC PACLEN

			for (i = 1; i <= TNC->HOSTSTREAMS; i++)
			{
				AllocateDEDChannel(TNC, i);			// Also used by Y command handler
			}

			TNC->Channels[0] = zalloc(sizeof(struct StreamInfo));
			memcpy(TNC->Channels[0], TNC->Channels[1], sizeof(struct StreamInfo));		// For monitoring

			break;

		case KANTRONICS:

			sprintf(pgm, "KANT %s", PNptr);

			if (TNC->HOSTSTREAMS == 0)
				TNC->HOSTSTREAMS = 1;		// Default

			for (i = 0; i <= TNC->HOSTSTREAMS; i++)
			{
				struct StreamInfo * Channel;

				// Use Stream zero for defaults
				
				TNC->Channels[i] = malloc(sizeof (struct StreamInfo));
				memset(TNC->Channels[i], 0, sizeof (struct StreamInfo));

				Channel = TNC->Channels[i];

				Channel->BPQStream = FindFreeStream();
				READCHANGE(Channel->BPQStream);					// Prevent Initial *** disconnected

				Debugprintf("BPQ32 KANT Init Stream %d  BPQ Stream %d", i, Channel->BPQStream );

	//			channel->Chan_TXQ = 0;
	//			channel->BPQStream = 0;
	//			channel->Connected = FALSE;
	//			channel->MYCall[0] = 0;

			} 
			break;
	
		case SCS:

			TNC->ECHOFLAG = 1;

			if (TNC->HOSTSTREAMS == 0)
				TNC->HOSTSTREAMS = 1;		// Default

			TNC->MALL = 1;
			TNC->MCOM = 1;
			TNC->MMASK = -1;			//  MONITOR MASK FOR PORTS
			TNC->TPACLEN = PACLEN;		// TNC PACLEN

			sprintf(pgm, "SCS %s", PNptr);

			for (i = 1; i <= TNC->HOSTSTREAMS; i++)
			{
				struct StreamInfo * Channel = zalloc(sizeof(struct StreamInfo));

				TNC->Channels[i] = Channel;

				Channel->BPQStream = FindFreeStream();
				READCHANGE(Channel->BPQStream);					// Prevent Initial *** disconnected

				Debugprintf("BPQ32 SCS Init Stream %d  BPQ Stream %d", i, Channel->BPQStream );
			}

			TNC->Channels[0] = zalloc(sizeof(struct StreamInfo));
			memcpy(TNC->Channels[0], TNC->Channels[1], sizeof(struct StreamInfo));		// For monitoring

			strcpy(pgm, "bpq32.exe");

			break;

		}

		if (Baud)
			TNC->Speed = atoi(Baud);
		else
			TNC->VCOM = TRUE;
			
		if (_memicmp(TNC->PORTNAME, "COM", 3) == 0)
		{
			TNC->ComPort = atoi(&TNC->PORTNAME[3]);
			TNC->VCOM = FALSE;
		}
		else
		{
			if (_memicmp(TNC->PORTNAME, "VCOM", 4) == 0)
				TNC->ComPort = atoi(&TNC->PORTNAME[4]);
		}
		if (TNC->VCOM == 0)
		{
			// Real port

			if (TNC->ComPort)
				TNC->hDevice = OpenCOMPort((VOID *)TNC->ComPort, TNC->Speed, TRUE, TRUE, FALSE, 0);
			else
				TNC->hDevice = OpenCOMPort(TNC->PORTNAME, TNC->Speed, TRUE, TRUE, FALSE, 0);

			TNC->PortEnabled = 1;   

				TNC->RTS = 1;
//				TNC->DTR = 1;
		}
		else
		{
			// VCOM Port			
#ifdef WIN32
			TNC->hDevice = BPQOpenSerialPort(TNC, &Errorval);
#else
			TNC->hDevice = LinuxOpenPTY(TNC->PORTNAME);
#endif
			if (TNC->hDevice != (HANDLE) -1)
			{            
				if (TNC->NewVCOM == 0)
				{
					resp = BPQSerialIsCOMOpen(TNC->hDevice, &OpenCount);
					TNC->PortEnabled = OpenCount;
				}

				resp = BPQSerialSetCTS(TNC->hDevice);
				resp = BPQSerialSetDSR(TNC->hDevice);
            
				TNC->CTS = 1;
				TNC->DSR = 1; 
			}
			else
			{
				Consoleprintf("TNC - Open Failed for Port %s", TNC->PORTNAME);
				TNC->hDevice = 0;
			}
		}

		if (TNC->hDevice)
		{
			// Set up buffer pointers

			TNC->PUTPTR = TNC->GETPTR = &TNC->RXBUFFER[0];
			TNC->RXCOUNT = 0;
			TNC->CURSOR = &TNC->TXBUFFER[0]; // RESET MESSAGE START
			TNC->MSGLEN = 0;
		
			TNC->VLSR = 0x20;
			TNC->VMSR = 0x30;

/*	PUSH	ECX

	MOV	ESI,OFFSET UNPROTOCALL
	CALL	DECODECALLSTRING

	LEA	EDI,UNPROTO[EBX]
	MOV	ECX,56
	REP MOVSB			; UNPROTO ADDR

	POP	ECX
*/

			APPLMASK = TNC->APPLICATION;
			ApplNum = 1;

			while  (APPLMASK && (APPLMASK & 1) == 0)
			{
				ApplNum++;
				APPLMASK >>= 1;
			}

			memcpy(TNC->MYCALL, &APPLCALLTABLE[ApplNum-1].APPLCALL_TEXT, 10);

			if (TNC->MYCALL[0] < '0')
				memcpy(TNC->MYCALL, MYNODECALL, 10);

			strlop(TNC->MYCALL, ' ');
		}

		TNC = TNC->Next;
	}

#ifdef LINBPQ
	strcpy(pgm, "LinBPQ");
#else
	strcpy(pgm, "bpq32.exe");
#endif
	Consoleprintf("TNC Emulator Init Complete");

	_beginthread(TNCBGThread,0,0);

	return TRUE;
}

VOID CloseTNCEmulator()
{
	struct TNCDATA * TNC = TNC2TABLE;		// malloc'ed
	int i, Stream;

	TNCRUNNING = FALSE;

	while (TNC)
	{
		if (TNC->Mode == TNC2)
		{
			Stream = TNC->BPQPort;
		
			SetAppl(Stream, 0, 0);
			Disconnect(Stream);
			READCHANGE(Stream);					// Prevent Initial *** disconnected
			DeallocateStream(Stream);
		}
		else
		{
			for (i = 1; i <= TNC->HOSTSTREAMS; i++)
			{
				Stream = TNC->Channels[i]->BPQStream;
		
				SetAppl(Stream, TNC->APPLFLAGS, 0);
				Disconnect(Stream);
				READCHANGE(Stream);					// Prevent Initial *** disconnected
				DeallocateStream(Stream);
			}
		}
		CloseCOMPort(TNC->hDevice);

		TNC = TNC->Next;
	}
}

VOID TNCTimer()
{
	// 100 Ms Timer

	struct TNCDATA * TNC = TNC2TABLE;
	struct StreamInfo * channel;
	int n;

	int NeedTrace = 0;

	while (TNC)
	{
		if (TNC->Mode != TNC2)
			goto NotTNC2;
			
		NeedTrace |= TNC->TRACEFLAG;				//SEE IF ANY PORTS ARE MONITORING

	//	CHECK FOR PACTIMER EXPIRY AND CMDTIME

		if (TNC->CMDTMR)
		{
			TNC->CMDTMR--;

			if (TNC->CMDTMR == 0)
			{
				//	CMDTMR HAS EXPIRED - IF 3 COMM CHARS RECEIVED, ENTER COMMAND MODE

				if (TNC->COMCOUNT == 3)
				{
					//	3 ESCAPE CHARS RECEIVED WITH GUARDS - LEAVE TRAN MODE

					SETCOMM00(TNC);

					goto TIM100;			//DONT RISK TRANSTIMER AND CMDTIME FIRING AT ONCE
				}

				TNC->CMDTMR = 0;			// RESET COUNT
				goto TIM100;

			}
		}

		if (TNC->TRANSTIMER)
		{
			TNC->TRANSTIMER--;
			if (TNC->TRANSTIMER == 0)
			{
				if (TNC->MSGLEN)				// ?MESSAGE ALREADY SENT
					SENDPACKET(TNC);
			}
		}
TIM100:
		//	CHECK FLOW CONTROL

		if ((TNC->VMSR & 0x20))			// ALREADY OFF?
		{
			CHECKCTS(TNC);				// No, so check
		}

		goto NextTNC;

NotTNC2:

		for (n = 1; n <= TNC->HOSTSTREAMS; n++)
		{
			channel = TNC->Channels[n];
			
			if (channel->CloseTimer)
			{
				channel->CloseTimer--;
				if (channel->CloseTimer == 0)
					Disconnect(channel->BPQStream);
			}
		}
NextTNC:
		TNC = TNC->Next;
	}
	DOMONITORING(NeedTrace);
}

#ifndef WIN32

int TNCReadCOMBlock(HANDLE fd, char * Block, int MaxLength, int * err)
{
	int Length;
	
	*err = 0;

	Length = read(fd, Block, MaxLength);

	if (Length < 0)
	{
		if (errno != 11 && errno != 35)					// Would Block
			*err = errno;

		return 0;
	}

	return Length;
}

#endif

VOID TNCPoll()
{
	unsigned int n;
	ULONG ConCount, ModemStat;
	char rxbuffer[1000];
	int retval, more;
	char TXMsg[1000];
	ULONG RXCount, TXCount, Read = 0, resp;

	struct TNCDATA * TNC = TNC2TABLE;		// malloc'ed

	while (TNC)
	{
		if (TNC->hDevice == 0)
		{
			TNC = TNC->Next;
			continue;						// Open failed
		}
		if (TNC->Mode == KANTRONICS)
		{
			// Have to poll for Data and State changes

			int n, len, count, state, change;
			struct StreamInfo * Channel;
			UCHAR Buffer[400];
			
			for (n = 1; n <= TNC->HOSTSTREAMS; n++)
			{
				Channel = TNC->Channels[n];

				SessionState(Channel->BPQStream, &state, &change);
		
				if (change == 1)
				{
					if (state == 1)
	
					// Connected
			
						KANTConnected(TNC, Channel, n);	
					else
						KANTDisconnected(TNC, Channel, n);
				}

				do
				{ 
					GetMsg(Channel->BPQStream, &Buffer[3], &len, &count);

					if (len > 0)
					{
						// If a failure, set a close timer (for Airmail, etc)

						if (strstr(&Buffer[3], "} Downlink connect needs port number") ||
							strstr(&Buffer[3], "} Failure with ") ||
							strstr(&Buffer[3], "} Sorry, "))
							Channel->CloseTimer = CloseDelay * 10;

						else
							Channel->CloseTimer = 0;			// Cancel Timer


						if (TNC->MODE)
						{
							Buffer[0] = 'D';
							Buffer[1] = '1';
							Buffer[2] = n + '@';
							SendKISSData(TNC, Buffer, len+3);
						}
						else
							BPQSerialSendData(TNC, &Buffer[3], len); 
					}
	  
				}
				while (count > 0);
			}
		}


#ifdef WIN32
		if (TNC->VCOM)
		{
			ConCount = 0;

			if (TNC->NewVCOM == 0)
			{
				BPQSerialIsCOMOpen(TNC->hDevice, &ConCount);

				if (TNC->PortEnabled == 1 && ConCount == 0)
		
					//' Connection has just closed - if connected, disconnect stream
			    
	//				if (BPQHOSTVECTOR[TNC->BPQPort-1].HOSTSESSION)
						SessionControl(TNC->BPQPort, 2, 0);

  
			    if (TNC->PortEnabled != ConCount)
				{
					TNC->PortEnabled = ConCount;
				}

				if (!TNC->PortEnabled)
				{
					TNC = TNC->Next;
					continue;
				}
			}

			if (TNC->Mode == KANTRONICS || TNC->Mode == SCS)
				resp = BPQSerialGetData(TNC, &TNC->RXBUFFER[TNC->RXBPtr], 1000 - TNC->RXBPtr, &Read);
			else
				resp = BPQSerialGetData(TNC, rxbuffer, 1000, &Read);

			if (Read)
			{
				if (TNC->Mode == TNC2)
				{		
					for (n = 0; n < Read; n++)
						TNC2PutChar(TNC, rxbuffer[n]);
				}
				else if (TNC->Mode == DED)
				{		
					for (n = 0; n < Read; n++)
						TfPut(TNC, rxbuffer[n]);
				}
				else if (TNC->Mode == KANTRONICS)
				{
					TNC->RXBPtr += Read;
					ProcessPacket(TNC, TNC->RXBUFFER, TNC->RXBPtr);
				}
				else if (TNC->Mode == SCS)
				{
					TNC->RXBPtr += Read;
					ProcessSCSPacket(TNC, TNC->RXBUFFER, TNC->RXBPtr);
				}
			}
			if (TNC->NewVCOM == 0)
			{
				resp = BPQSerialGetQCounts(TNC->hDevice, &RXCount, &TXCount);
				if (TXCount > 4096) goto getstatus;
			}

			n=0;

		getloop:

			TNC2GetChar(TNC, &retval, &more);

			if (retval != -1)
				TXMsg[n++] = retval;

			if (more > 0 && n < 1000) goto getloop;
            
			if (n > 0)
				BPQSerialSendData(TNC, TXMsg, n);

		getstatus:

			TNC2GetVMSR(TNC, &retval);
        
			if ((retval & 8) == 8)	 //' DCD (Connected) Changed
			{	
				TNC->DCD = (retval & 128) / 128;
				
				if (TNC->DCD == 1)
					BPQSerialSetDCD(TNC->hDevice);
				else
					BPQSerialClrDCD(TNC->hDevice);
			}

			if ((retval & 1) == 1)  //' CTS (Flow Control) Changed
			{			
				TNC->CTS = (retval & 16) / 16;
        
				if (TNC->CTS == 1)
					BPQSerialSetCTS(TNC->hDevice);
				else
					BPQSerialClrCTS(TNC->hDevice);

			}

			BPQSerialGetDTRRTS(TNC->hDevice,&ModemStat);
			

			if ((ModemStat & 1) != TNC->DTR)
			{
				TNC->DTR=!TNC->DTR;
			}

			if ((ModemStat & 2) >> 1 != TNC->RTS)
			{
				TNC->RTS=!TNC->RTS;
			}

			TNC = TNC->Next;
			continue;
		}
#endif
		{
			// Real Port or Linux Virtual 
			
			int Read, n;
			int retval, more;
			char TXMsg[500];
#ifndef WIN32
			int err;

			// We can tell if partner has gone on PTY Pair - read returns 5

			if (TNC->Mode == KANTRONICS || TNC->Mode == SCS)
				Read = TNCReadCOMBlock(TNC->hDevice, &TNC->RXBUFFER[TNC->RXBPtr], 1000 - TNC->RXBPtr, &err);
			else
				Read = TNCReadCOMBlock(TNC->hDevice, rxbuffer, 1000, &err);

			if (err)
			{
				if (TNC->PortEnabled)
				{
					TNC->PortEnabled = FALSE;
					DisableAppl(TNC);
					Debugprintf("Device %s closed", TNC->PORTNAME);
				}
			}
			else
				TNC->PortEnabled = TRUE;

#else
			if (TNC->Mode == KANTRONICS || TNC->Mode == SCS)
				Read = ReadCOMBlock(TNC->hDevice, &TNC->RXBUFFER[TNC->RXBPtr], 1000 - TNC->RXBPtr);
			else
				Read = ReadCOMBlock(TNC->hDevice, rxbuffer, 1000);
#endif

			if (Read)
			{		
				if (TNC->Mode == TNC2)
				{		
					for (n = 0; n < Read; n++)
						TNC2PutChar(TNC, rxbuffer[n]);
				}
				else if (TNC->Mode == DED)
				{		
					for (n = 0; n < Read; n++)
						TfPut(TNC, rxbuffer[n]);
				}
				else if (TNC->Mode == KANTRONICS)
				{
					TNC->RXBPtr += Read;
					ProcessPacket(TNC, TNC->RXBUFFER, TNC->RXBPtr);
				}
				else if (TNC->Mode == SCS)
				{
					TNC->RXBPtr += Read;
					ProcessSCSPacket(TNC, TNC->RXBUFFER, TNC->RXBPtr);
				}
			}

			n=0;

		getloopR:

			TNC2GetChar(TNC, &retval, &more);

			if (retval != -1)
				TXMsg[n++] = retval;

			if (more > 0 && n < 500) goto getloopR;
    
			if (n > 0) 

			{
				resp = WriteCOMBlock(TNC->hDevice, TXMsg, n);
			}
		}
		TNC = TNC->Next;
	}
}

int APIENTRY SetTraceOptionsEx(int mask, int mtxparam, int mcomparam, int monUIOnly);


VOID DOMONITORING(int NeedTrace)
{
	//	IF ANY PORTS HAVE MONITOR ENABLED, SET MONITOR BIT ON FIRST PORT

	struct TNCDATA * TNC = TNC2TABLE;		// malloc'ed
	int Tracebit = 0, len, count, n;
	time_t Stamp;
	ULONG SaveMMASK = MMASK;
	BOOL SaveMTX = MTX;
	BOOL SaveMCOM = MCOM;
	BOOL SaveMUI = MUIONLY;

	if (NeedTrace)
		Tracebit = 0x80;

	if (TNC->CONOK)
		SetAppl(TNC->BPQPort, TNC->APPLFLAGS | Tracebit, TNC->APPLICATION);
	else
		SetAppl(TNC->BPQPort, TNC->APPLFLAGS | Tracebit, 0);

	Stamp = GetRaw(TNC->BPQPort, (char *)&MONITORDATA, &len, &count);

	if (len == 0)
		return;

	len = DecodeFrame(&MONITORDATA, MONBUFFER, (int)Stamp);
	
	while (TNC)
	{
		if (TNC->Mode == TNC2 && TNC->TRACEFLAG)
		{
			SetTraceOptionsEx(TNC->MMASK, TNC->MTX, TNC->MCOM, 0);
			len = IntDecodeFrame(&MONITORDATA, MONBUFFER, (UINT)Stamp, TNC->MMASK, FALSE, FALSE);
//			printf("%d %d %d %d %d\n", len, MMASK, MTX, MCOM, MUIONLY);
			SetTraceOptionsEx(SaveMMASK, SaveMTX, SaveMCOM, SaveMUI);

			if (len)
			{
				for (n = 0; n < len; n++)
				{
					PUTCHARINBUFFER(TNC, MONBUFFER[n]);
				}
			}
		}
		TNC=TNC->Next;
	}
}


VOID TNC2PutChar(struct TNCDATA * TNC, int Char)
{
	if (TNC->MODEFLAG & COMMAND)
		goto KEYB06C;				// COMMAND MODE - SKIP TRANS TEST
		
	if (TNC->MODEFLAG & TRANS)
		goto KEYB06T;				//  TRANS MODE	
	
	//	CONV MODE - SEE IF CPACTIME ON

	if (TNC->CPACTIME)
		TNC->TRANSTIMER = TRANSDELAY;

	goto KEYB06;					// PROCESS CHAR

KEYB06T:

//	Transparent Mode - See if Escape Sequence Received (3 esc chars, with guard timer)

//	CHECK FOR COMMAND CHAR IF CMDTIME EXPIRED OR COMAND CHAR ALREADY RECEIVED
	
	if (TNC->COMCOUNT)
		goto KBTRN3;					// ALREADY GOT AT LEAST 1

	if (TNC->CMDTMR)
		goto KBTRN5;		    		// LESS THAN CMDTIME SINCE LAST CHAR

KBTRN3:

	if (Char != TNC->COMCHAR)
	{
		TNC->COMCOUNT = 0;
		goto KBTRN5;				// NOT COMMAND CHAR
	}

	TNC->COMCOUNT++;

KBTRN5:

	TNC->CMDTMR	= TNC->CMDTIME;		// REPRIME ESCAPE TIMER

	TNC->TRANSTIMER = TRANSDELAY;

	KBNORM(TNC, Char);
	return;						// TRANSPARENT MODE

KEYB06:

//	STILL JUST CONV MODE

	if (Char != TNC->SENDPAC)
		goto NOTSENDPAC;

//	SEND PACKET CHAR - SHOUD WE SEND IT?

	TNC->TRANSTIMER = 0;

	if (TNC->CRFLAG)
		KBNORM(TNC, Char);	// PUT CR IN BUFFER				
	
	SENDPACKET(TNC);
	return;


NOTSENDPAC:
KEYB06C:

//	COMMAND OR CONV MODE

	if (Char < 32)			//  control
	{
		if (Char == 10 && TNC->LFIGNORE)
			return;

		if (Char == 8)
		{
			if (TNC->MSGLEN == 0)
				return;

			TNC->MSGLEN--;
			TNC->CURSOR--;
		
			if (TNC->ECHOFLAG)
				KBECHO(TNC, Char);
	
			return;
		}

		if (Char == 26)			// Ctrl/Z
		{
			KBNORM(TNC, Char);		// FOR MBX TERMINATOR
			return;
		}

		if (Char == TNC->COMCHAR)
		{
			SETCOMMANDMODE(TNC);
			return;
		}
	
		if (TNC->MODEFLAG & COMMAND)
		{	
			if (Char == 0x14)			// CTRL/T
			{
				TNC->TRACEFLAG ^= 1;
				return;
			}

			if (Char == 13)
			{	
				KBNORM(TNC, 13);			// PUT CR IN BUFFER
				SENDPACKET(TNC);
				return;
			}
		}
		KBNORM(TNC, Char);					// Process others as normal chars
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               	KBNORM(TNC, Char);
}

VOID KBNORM(struct TNCDATA * TNC, int Char)
{
	if (TNC->MSGLEN > 256)
		goto TOOLONG;			// PROTECT BUFFER

	*(TNC->CURSOR++) = Char;
	TNC->MSGLEN++;

TOOLONG:

	if (TNC->ECHOFLAG)
		KBECHO(TNC, Char);

	if (TNC->MSGLEN < TNC->TPACLEN)
		return;;

//	DONT APPLY PACLEN IN COMMAND MODE

	if (TNC->MODEFLAG & COMMAND)
		return;

	SENDPACKET(TNC);			// Send what we have
}


VOID SETCOMMANDMODE(struct TNCDATA * TNC)
{
	if (TNC->MSGLEN)
		SENDPACKET(TNC);

	SETCOMM00(TNC);
}

VOID SETCOMM00(struct TNCDATA * TNC)
{
	TNC->MODEFLAG |= COMMAND;			// BACK TO COMMAND MODE
	TNC->MODEFLAG &= ~(CONV+TRANS);
	TNC->TRANSTIMER = 0;				// CANCEL TRANS MODE SEND TIMER
	TNC->AUTOSENDFLAG = 0;				// IN CASE ALREADY SET

	SENDREPLY(TNC, CMDMSG, 4);

	TNC->CURSOR = &TNC->TXBUFFER[0]; // RESET MESSAGE START
	TNC->MSGLEN = 0;
}



VOID SENDPACKET(struct TNCDATA * TNC)
{
	//	SEE IF COMMAND STATE

	int Stream = 0;			// Unprooto

	if (TNC->MODEFLAG & COMMAND) 
	{
		TNCCOMMAND(TNC);				// COMMAND TO TNC
		TNC->CURSOR = &TNC->TXBUFFER[0];		// RESET MESSAGE START		
		TNC->MSGLEN = 0;
		return;
	}

	//	IF CONNECTED, SEND TO L4 (COMMAND HANDLER OR DATA),
	//	   OTHERWISE SEND AS AN UNPROTO FRAME (TO ALL PORTS)


	if (TNC->VMSR & 0x80)				// CONNECTED?
		Stream = TNC->BPQPort;

	SendMsg(Stream, TNC->TXBUFFER, TNC->MSGLEN);
	
	TNC->CURSOR = &TNC->TXBUFFER[0];		// RESET MESSAGE START		
	TNC->MSGLEN = 0;

	CHECKCTS(TNC);						// SEE IF NOW BUSY

	return;
}

VOID KBECHO(struct TNCDATA * TNC, int Char)
{
	PUTCHARINBUFFER(TNC, Char);
}

VOID TNCCOMMAND(struct TNCDATA * TNC)
{
	//	PROCESS COMMAND TO TNC CODE
	
	char * ptr1, * ptr2;
	int n;
	CMDX * CMD;

	*(--TNC->CURSOR) = 0;

	_strupr(TNC->TXBUFFER);
	strcat(TNC->TXBUFFER, "         "); 

	ptr1 = &TNC->TXBUFFER[0];		//

	n = 10;
	
	while ((*ptr1 == ' ' || *ptr1 == 0) && n--)
		ptr1++;						// STRIP LEADING SPACES and nulls (from keepalive)

	if (n == -1)
	{
		// Null command

		SENDREPLY(TNC, CMDMSG, 4);
		return;
	}

	ptr2 = ptr1;				// Save

	CMD = &COMMANDLIST[0];
	n = 0;
	
	for (n = 0; n < NUMBEROFTNCCOMMANDS; n++)
	{
		int CL = CMD->CMDLEN;

		ptr1 = ptr2;

		// ptr1 is input command

		if (memcmp(CMD->String, ptr1, CL) == 0)
		{
			// Found match so far - check rest
		
			char * ptr2 = &CMD->String[CL];
			
			ptr1 += CL;

			if (*(ptr1) != ' ')
			{
				while(*(ptr1) == *ptr2 && *(ptr1) != ' ')
				{
					ptr1++;
					ptr2++;
				}
			}

			if (*(ptr1) == ' ')
			{
				ptr1++;						// Skip space

				CMD->CMDPROC(TNC, ptr1, CMD);
				SENDREPLY(TNC, CMDMSG, 4);

				return;
			}
		}
		
		CMD++;
	
	}

	SENDREPLY(TNC, WHATMSG, 8);

}

/*
;
UNPROTOCMD:
;
;	EXTRACT CALLSIGN STRING 
;
	CMP	BYTE PTR [ESI],20H
	JE	UNPROTODIS

	CMP	BYTE PTR [ESI],"*"
	JE	CLEARUNPROTO

	CALL	DECODECALLSTRING	; CONVERT TO AX25 FORMAT

	JZ	UNPROTOOK

	JMP	TNCDUFF

CLEARUNPROTO:

	LEA	EDI,UNPROTO[EBX]
	MOV	AL,0
	MOV	ECX,63
	REP STOSB			; COPY IN

UNPROTODIS:

	MOV	AL,1
	CALL	DISPLAYUNPROTO		; DISPLAY CURRENT SETTING
	JMP	SENDOK

UNPROTOOK:

	PUSH	ESI
	MOV	AL,0
	CALL	DISPLAYUNPROTO		; DISPLAY OLD STRRING
	POP	ESI

	LEA	EDI,UNPROTO[EBX]
	MOV	ECX,63
	REP MOVSB			; COPY IN
CONMODE:
	JMP	SENDOK
;CONMODE:
	JMP	KBRET

DISPLAYUNPROTO:
;
	PUSH	EAX

	MOV	ESI,OFFSET UNPROT
	MOV	ECX,8
	CALL	PUTSTRINGINBUFFER

	MOV	ESI,OFFSET WAS		; DISPLAY "was"
	MOV	ECX,5

	POP	EAX
	OR	AL,AL
	JZ	DISPU00			; NO

	MOV	ECX,1			; LEAVE OUT "WAS"

DISPU00:

	CALL	PUTSTRINGINBUFFER

	LEA	ESI,UNPROTO[EBX]
	CMP	BYTE PTR [ESI],40H
	JBE	DISPUPRET

	CALL	CONVFROMAX25

	PUSH	ESI

	MOV	ESI,OFFSET NORMCALL

	CALL	PUTSTRINGINBUFFER

	POP	ESI

	CMP	BYTE PTR [ESI],0
	JE	DISPUPRET

	PUSH	ESI

	MOV	ESI,OFFSET VIA
	MOV	ECX,5
	CALL	PUTSTRINGINBUFFER

	POP	ESI

DISPUPLOOP:

	CALL	CONVFROMAX25

	PUSH	ESI
	MOV	ESI,OFFSET NORMCALL
	INC	ECX
	CALL	PUTSTRINGINBUFFER
	POP	ESI

	CMP	BYTE PTR [ESI],0
	JNE	DISPUPLOOP


DISPUPRET:
	MOV	AL,0DH
	CALL	PUTCHARINBUFFER
	RET


BTEXT:
;
	CMP	BYTE PTR [ESI],20H
	JE	BTDIS
;
	PUSH	ESI
	MOV	AL,0
	CALL	DISPLAYBT		; DISPLAY OLD STRING
	POP	ESI

	MOV	EDI,OFFSET BTEXTFLD
	MOV	ECX,255
BTLOOP:
	LODSB
	STOSB
	CMP	ESI,CMDENDADDR		; END?
	JE	BTEND

	LOOP	BTLOOP
BTEND:
	XOR	AL,AL
	STOSB				; NULL ON END
;
;	SET UP TO SEND IT AS A UI
;
	MOV	ECX,EDI
	MOV	ESI,OFFSET BTHDDR
	SUB	ECX,ESI
	MOV	MSGLENGTH[ESI],CX
;
;	PASS TO SWITCH
;
	MOV	ESI,OFFSET BTEXTFLD
	SUB	ECX,6			; DONT NEED HEADER

	MOV	AH,12			; UPDATE FUNCTIONS
	MOV	DX,1			; UPDATE BT

	CALL	NODE			; PASS TO NODE

	JMP	SENDOK

BTDIS:
	MOV	AL,1
	CALL	DISPLAYBT		; DISPLAY CURRENT SETTING
	JMP	SENDOK


DISPLAYBT:
;
	PUSH	EAX

	MOV	ESI,OFFSET BTCMD
	MOV	ECX,8
	CALL	PUTSTRINGINBUFFER

	MOV	ESI,OFFSET WAS		; DISPLAY "was"
	MOV	ECX,5

	POP	EAX
	OR	AL,AL
	JZ	DISPBT00		; NO

	MOV	ECX,1			; LEAVE OUT "WAS"

DISPBT00:
	CALL	PUTSTRINGINBUFFER

	MOV	ESI,OFFSET BTEXTFLD
DISPBT10:
	LODSB
	OR	AL,AL
	JZ	DISPBT20

	CALL	PUTCHARINBUFFER

	JMP	DISPBT10

DISPBT20:
	MOV	AL,0DH
	CALL	PUTCHARINBUFFER
	RET


*/

VOID DOCONMODECHANGE(struct TNCDATA * TNC)
{

	TNC->VMSR |= 0x88;		// SET CONNECTED

	//	IF NOMODE IS ON LEAVE IN TNC COMMAND MODE, ELSE PUT INTO CONV MODE
	//		(MAY NEED TO IMPLEMENT CONMODE SOMETIME)

	if (TNC->NOMODE)
		return;

	TNC->MODEFLAG |= CONV;		// INTO CONVERSE MODE
	TNC->MODEFLAG &= ~(COMMAND+TRANS);
}

VOID SENDREPLY(struct TNCDATA * TNC, char * Msg, int Len)
{
	int n = 0;
	
	for (n= 0; n < Len; n++)
	{
		PUTCHARINBUFFER(TNC, Msg[n]);
	}
}


VOID SEND_CONNECTED(struct TNCDATA * TNC)
{
	//	SEND TAPR-STYLE *** CONNECTED TO CURRENT PORT

	int len;
	char Response[128];
	char Call[11] = "";
	int paclen, dummy;

	GetConnectionInfo(TNC->BPQPort, Call, &dummy, &dummy, &paclen, &dummy, &dummy);
	
	if (paclen)
		TNC->TPACLEN = paclen;

	if (TNC->MODEFLAG & TRANS)
		return;					//NOT IF TRANSPARENT

	strlop(Call, ' ');

	if (TNC->CBELL)
		len = sprintf(Response, "%s%s%c\r", CONMSG, Call, 7);		// Add BELL char
	else
		len = sprintf(Response, "%s%s\r", CONMSG, Call);

	SENDREPLY(TNC, Response, len);
}

VOID PUTCHARINBUFFER(struct TNCDATA * TNC, int Char)
{
	//	CALLED BY L4 CODE TO PASS DATA TO VIRTUAL TNC
	;
	if (TNC->RXCOUNT >= TNCBUFFLEN)
	{
		//	OVERRUN - LOSE IT

		TNC->VLSR |= 2;			// SET OVERRUN ERROR
		return;
	}

	TNC->VLSR &= ~2;			// CLEAR OVERRRUN

	*(TNC->PUTPTR++) = Char;
	TNC->RXCOUNT++;

	if (TNC->PUTPTR == &TNC->RXBUFFER[TNCBUFFLEN])
		TNC->PUTPTR = &TNC->RXBUFFER[0];

	if(TNC->RXCOUNT > TNCBUFFLEN-300)	// ALLOW FOR FULL PACKET
	{			
		//	BUFFER GETTING FULL - DROP RTS/DTR

		TNC->RTSFLAG |= 1;				// SET BUSY	
	}

	if (Char == 13 && TNC->AUTOLF)
		PUTCHARINBUFFER(TNC, 10);		// Add LF
}


VOID CHECKCTS(struct TNCDATA * TNC)
{
	//	SEE IF CROSS-SESSION STILL BUSY

	if (RXCount(TNC->BPQPort) > 4)
	{
		// Busy

		if ((TNC->VMSR & 0x10) == 0)	// ALREADY OFF?
			return;						// No Change

		TNC->VMSR &= 0xef;				// Drop CTS
		TNC->VMSR |= 1;					// Delta bit
		return;
	}

	// Not busy

	if (TNC->VMSR & 0x10)			// ALREADY ON?
		return;						// No Change

	TNC->VMSR |= 0x11;				//  CTS AND DELTA CTS
}



VOID CONNECTTONODE(struct TNCDATA * TNC)
{
	char AXCALL[7];
	
	ConvToAX25(TNC->MYCALL, AXCALL);
		
	SessionControl(TNC->BPQPort, 1, TNC->APPLICATION);
	ChangeSessionCallsign(TNC->BPQPort, AXCALL);
}
	

VOID GETDATA(struct TNCDATA * TNC)
{
	int state, change, InputLen, count, n;
	char InputBuffer[512];

	//	LOOK FOR STATUS CHANGE
	
	LocalSessionState(TNC->BPQPort, &state, &change, TRUE);
		
	if (change == 1)
	{
		if (state == 1) // Connected	
		{
			SEND_CONNECTED(TNC);
			DOCONMODECHANGE(TNC);			// SET CONNECTED AND CHANGE MODE IF NEEDED
		}
		else
		{
			TNC->MODEFLAG |= COMMAND;
			TNC->MODEFLAG &= ~(CONV+TRANS);

			TNC->VMSR &= 0x7F;			// DROP DCD
			TNC->VMSR |= 8;				// DELTA DCD

			SENDREPLY(TNC, DISCONNMSG, 18);
		}
	}
	else
	{
		// No Change

		//	VERIFY CURRENT STATE

		if (state == 1) // Connected	
		{
			//	SWITCH THINKS WE ARE CONNECTED

			if ((TNC->VMSR & 0x80) == 0)
			{
				// TNC DOesnt

				SEND_CONNECTED(TNC);
				DOCONMODECHANGE(TNC);			// SET CONNECTED AND CHANGE MODE IF NEEDED
			}
		}
		else
		{
			// SWITCH THINKS WE ARE DISCONNECTED

			if (TNC->VMSR & 0x80)
			{
				// We Dissagree, so force off

				TNC->MODEFLAG |= COMMAND;
				TNC->MODEFLAG &= ~(CONV+TRANS);

				TNC->VMSR &= 0x7F;				// DROP DCD
				TNC->VMSR |= 8;				// DELTA DCD

				SENDREPLY(TNC, DISCONNMSG, 18);
			}
		}
	}

	// SEE IF ANYTHING QUEUED

	if (TNC->RTSFLAG & 1)
		return;

	GetMsg(TNC->BPQPort, InputBuffer, &InputLen, &count);

	if (InputLen == 0)
		return;

	for (n = 0; n < InputLen; n++)
	{
		PUTCHARINBUFFER(TNC, InputBuffer[n]);
	}
}

// DED Mode Support

unsigned char PARAMREPLY[]="* 0 0 64 10 4 4 10 100 18000 30 2 0 2\r\n";

#define PARAMPORT PARAMREPLY[2]

#define LPARAMREPLY	39

unsigned char BADCMDREPLY[]="\x2" "INVALID COMMAND\x0";

#define LBADCMDREPLY 17 //sizeof BADCMDREPLY

unsigned char DATABUSYMSG[]="\x2" "TNC BUSY - LINE IGNORED\x0";
#define LDATABUSY 25

unsigned char BADCONNECT[]="\x2" "INVALID CALLSIGN\x0";
#define LBADCONNECT	18

unsigned char BUSYMSG[]="BUSY fm SWITCH\x0";

//unsigned char CONSWITCH[]="\x3" "(1) CONNECTED to           \x0";

unsigned char DEDSWITCH[]="\x1" "0:SWITCH    \x0";
#define LSWITCH	14
	
unsigned char NOTCONMSG[]="\x1" "CHANNEL NOT CONNECTED\x0";
#define LNOTCON	23

unsigned char ALREADYCONMSG[]="You are already connected on another port\r";
#define ALREADYLEN	45


byte * EncodeCall(byte * Call);
VOID SENDENFORCINGPACLEN(struct StreamInfo * Channel, char * Msg, int Len);
VOID SENDCMDREPLY(struct TNCDATA * TNC, char * Msg, int Len);
int APIENTRY SetTraceOptionsEx(int mask, int mtxparam, int mcomparam, int monUIOnly);
int DOCOMMAND(struct TNCDATA * conn);
int PROCESSPOLL(struct TNCDATA * TNC, struct StreamInfo * Channel);




VOID PUTSTRING(struct TNCDATA * conn, UCHAR * Msg)
{
	int len = strlen(Msg);

	while (len)
	{
		*(conn->PUTPTR++) = *(Msg++);

		if (conn->PUTPTR == &conn->RXBUFFER[TNCBUFFLEN])
			conn->PUTPTR = (UCHAR *)&conn->RXBUFFER;

		conn->RXCOUNT++;

		len--;
	}
}


int PUTCHARx(struct TNCDATA * conn, UCHAR c)
{
	*(conn->PUTPTR++) = c;

	if (conn->PUTPTR == &conn->RXBUFFER[TNCBUFFLEN])
		conn->PUTPTR = (UCHAR *)&conn->RXBUFFER;

	conn->RXCOUNT++;
	
	return 0;
}



VOID DisableAppl(struct TNCDATA * TNC)
{
	int i, Stream;

	for (i = 1; i <= TNC->HOSTSTREAMS; i++)
	{
		Stream = TNC->Channels[i]->BPQStream;
		
		SetAppl(Stream, TNC->APPLFLAGS, 0);
		Disconnect(Stream);
		READCHANGE(Stream);					// Prevent Initial *** disconnected
	}
}

VOID EnableAppl(struct TNCDATA * TNC)
{
	int i;

	for (i = 1; i <= TNC->HOSTSTREAMS; i++)
	{
		SetAppl(TNC->Channels[i]->BPQStream, TNC->APPLFLAGS, TNC->APPLICATION);
	}
}

BOOL TfPut(struct TNCDATA * TNC, UCHAR character) 
{
	struct StreamInfo * Channel;
	TRANSPORTENTRY * L4 = NULL;

	if (!TNC->MODE)
		goto CHARMODE;

//	HOST MODE

	if (TNC->HOSTSTATE == 0)
	{
		TNC->MSGCHANNEL = character;
		TNC->HOSTSTATE++;
		return TRUE;
	}

	if (TNC->HOSTSTATE == 1)
	{
		TNC->MSGTYPE = character;
		TNC->HOSTSTATE++;
		return TRUE;
	}

	if (TNC->HOSTSTATE == 2)
	{
		TNC->MSGCOUNT = character;
		TNC->MSGLENGTH = character;
		TNC->MSGCOUNT++;
		TNC->MSGLENGTH++;
		TNC->HOSTSTATE++;

		TNC->DEDCURSOR = &TNC->DEDTXBUFFER[0];
		return TRUE;
	}

//	RECEIVING COMMAND/DATA

	*(TNC->DEDCURSOR++) = character;

	TNC->MSGCOUNT--;

	if (TNC->MSGCOUNT)
		return TRUE;				// MORE TO COME

	TNC->HOSTSTATE=0;

	if (TNC->MSGCHANNEL <= TNC->HOSTSTREAMS)
		Channel = TNC->Channels[TNC->MSGCHANNEL];
	else
		Channel = TNC->Channels[1];

	PROCESSHOSTPACKET(Channel, TNC);

	TNC->HOSTSTATE = 0;

	return TRUE;


CHARMODE:

	if (character == 0x11) return TRUE;

	if (character == 0x18)
	{
		//	CANCEL INPUT
 
		TNC->CURSOR = (UCHAR *)&TNC->TXBUFFER;
		
		return(TRUE);
	}

	*(TNC->CURSOR++) = character;

	if (character == 1 && (TNC->CURSOR > &TNC->TXBUFFER[4]) && *(TNC->CURSOR - 2) == 1 && *(TNC->CURSOR - 3) == 1)
	{
		// Looks like a resync request - Appl thinks we are in host mode

		TNC->MODE = 1;
		TNC->CURSOR = (UCHAR *)&TNC->TXBUFFER;
		EnableAppl(TNC);
		
		return(TRUE);
	}


	if (TNC->CURSOR == &TNC->TXBUFFER[300])
		TNC->CURSOR--;

	if (character == 0x0d)
	{
		//	PROCESS COMMAND (UNLESS HOST MODE)

		*(TNC->CURSOR++) = 0;

		DOCOMMAND(TNC);
	}
	return TRUE;
}
	

PROCESSHOSTPACKET(struct StreamInfo * Channel, struct TNCDATA * TNC)
{
	UCHAR * TXBUFFERPTR;
	int i;
	int Work;
	char WorkString[256];
	int State, Change, Count;
	TRANSPORTENTRY * L4 = NULL;
	unsigned char * MONCURSOR=0;

	TXBUFFERPTR = &TNC->DEDTXBUFFER[0];

	if ((UINT)Channel->Chan_TXQ == 0xffffffff)
	{
		Channel->Chan_TXQ = 0;
	}
		
	if (TNC->MSGTYPE != 0)
		goto NOTDATA;

	goto HOSTDATAPACKET;

//HOSTCMDS:
//	DD	'G','I', 'J', 'C', 'D', 'L', '@',      'Y', 'M'
//	DD	POLL,ICMD,JCMD,CCMD,DCMD,LCMD,ATCOMMAND,YCMD,HOSTMON

NOTDATA:

	if (TNC->DEDTXBUFFER[0] == 1)
	{
		// recovering

//		if (!TNC->Recovering)
//		{
//			sprintf(msg, "Port %d DED Recovery started\n", TNC->ComPort);
//			OutputDebugString(msg);
//			TNC->Recovering = TRUE;
//		}
	}
	else
	{
		// Not recovery
				
//		if (TNC->Recovering)
//		{
//			sprintf(msg, "Port %d DED Recovery completed\n", TNC->ComPort);
//			OutputDebugString(msg);
//			TNC->Recovering = FALSE;
//		}

	}

	if (TNC->DEDTXBUFFER[0] == 1)
		goto DUFFHOSTCMD;

//	sprintf(msg,"DED CMD: Port %d  CMD %c MSGCHANNEL %d\n", TNC->ComPort, TNC->TXBUFFER[0], MSGCHANNEL);
//	OutputDebugString(msg);

	if (_memicmp(TNC->DEDTXBUFFER, "QRES", 4 == 0))
		goto SENDHOSTOK;

	switch (toupper(TNC->DEDTXBUFFER[0]))
	{
	case 1:
		
		goto DUFFHOSTCMD;

	case 'G':

		goto POLL;

	case 'I':
		goto ICMD;
	
	case 'J':

		TNC->MODE = TNC->DEDTXBUFFER[5] & 1;

		if (TNC->MODE)
			EnableAppl(TNC);
		else
			DisableAppl(TNC);

		goto SENDHOSTOK;

	case 'C':
		goto CCMD;

	case 'D':
		goto DCMD;

	case 'L':
		goto LCMD;

	case '@':
		goto ATCOMMAND;

	case 'Y':
		goto YCMD;
	
	case 'E':
		goto ECMD;

	case 'M':

		if (TNC->DEDTXBUFFER[1] == 'N')
			goto DISABLEMONITOR;

		goto	ENABLEMONITOR;

	case 'K':
	case 'O':
		goto SENDHOSTOK;

	case 'V':					// Vesrion

		PUTCHARx(TNC, TNC->MSGCHANNEL);
		PUTCHARx(TNC, 1);
		PUTSTRING(TNC, "DSPTNC Firmware V.1.3a, (C) 2005-2010 SCS GmbH & Co.");
		PUTCHARx(TNC, 0);

		return TRUE;

	default:
		goto SENDHOSTOK;

ATCOMMAND:

	if (TNC->DEDTXBUFFER[1] == 'B')
		goto BUFFSTAT;

	if (TNC->DEDTXBUFFER[1] == 'M')
		goto BUFFMIN;

// Not recognised

	PUTCHARx(TNC, TNC->MSGCHANNEL);

	for (i=0; i < LBADCMDREPLY; i++)
	{
		PUTCHARx(TNC, BADCMDREPLY[i]);
	}

	return TRUE;


BUFFMIN:

	Work = MINBUFFCOUNT;
	goto BUFFCOMM;

BUFFSTAT:

	Work = QCOUNT;

BUFFCOMM:

	PUTCHARx(TNC, TNC->MSGCHANNEL);		// REPLY ON SAME CHANNEL
	PUTCHARx(TNC, 1);

	sprintf(WorkString, "%d", Work);		// Buffer count

	PUTSTRING(TNC, WorkString);

	PUTCHARx(TNC, 0);
	return TRUE;

ICMD:

	{
		char * Call = &TNC->DEDTXBUFFER[1];
		int len;
		char Reply[80];
		char ReplyCall[10];


	if (TNC->MSGLENGTH > 2)
	{
		//	Save callsign

		TNC->DEDTXBUFFER[TNC->MSGLENGTH] = 0;

		if (*Call == ' ')
			*Call++;			// May have leading space

		_strupr(Call);

		memset(Channel->MYCall, ' ', 10);
		memcpy(Channel->MYCall, Call, strlen(Call));

		Debugprintf("DED Host I chan %d call %s", TNC->MSGCHANNEL, Call);

		strcpy(ReplyCall, Call);

/*
	if (TNC->MSGCHANNEL == 0)		// if setting zero, copy to all others
	{
		int i;

		for (i = 1; i <= TNC->HOSTSTREAMS; i++)
		{
			memcpy(TNC->Channels[i]->MYCall, TNC->Channels[0]->MYCall, 10);
			Debugprintf("DED Capy to chan %d call %s", i, Channel->MYCall);
		}
	}
*/
	





	}
	else
	{
		memcpy(ReplyCall, Channel->MYCall, 10);
		strlop(ReplyCall, ' ');
	}

	len = sprintf(Reply, "\x2%s", ReplyCall);

	SENDCMDREPLY(TNC, Reply, len + 1);		// include the null

	return TRUE;
	}
ECMD:

	goto SENDHOSTOK;

DUFFHOSTCMD:

	PUTCHARx(TNC, TNC->MSGCHANNEL);

	for (i=0; i < LBADCMDREPLY; i++)
	{
		PUTCHARx(TNC, BADCMDREPLY[i]);
	}

	return TRUE;

ENABLEMONITOR:

	TNC->TRACEFLAG = 0x80;
	goto MONCOM;

DISABLEMONITOR:

	TNC->TRACEFLAG = 0;

MONCOM:

	SetAppl(TNC->Channels[0]->BPQStream, 2 | TNC->TRACEFLAG, TNC->APPLICATION);

	goto SENDHOSTOK;

CCMD:

//	CONNECT REQUEST

	if (TNC->MSGCHANNEL == 0)
		goto SENDHOSTOK;				// SETTING UNPROTO ADDR - JUST ACK IT

	*TNC->DEDCURSOR = 0;

	if (TNC->MSGLENGTH > 1)
		goto REALCALL;

//	STATUS REQUEST - IF CONNECTED, GET CALL

	DEDSWITCH[3] = 0;

	GetCallsign(Channel->BPQStream, &DEDSWITCH[3]);

	Debugprintf("CCMD %d %d %s", TNC->MSGCHANNEL, TNC->Channels[TNC->MSGCHANNEL]->BPQStream, &DEDSWITCH[3]);

	if (DEDSWITCH[3] == 0)
		SENDCMDREPLY(TNC, NOTCONMSG, LNOTCON);
	else
		SENDCMDREPLY(TNC, DEDSWITCH, LSWITCH);

	return TRUE;

REALCALL:

//	If to Switch, just connect, else pass c command to Node

	Debugprintf("CCMD %d %s", TNC->MSGCHANNEL, TXBUFFERPTR);

	Connect(Channel->BPQStream);

//	CONNECT WILL BE REPORTED VIA NORMAL STATUS CHANGE

	if (Channel->MYCall[0] > ' ')
		ChangeSessionCallsign(Channel->BPQStream, EncodeCall(Channel->MYCall));
	else
		ChangeSessionCallsign(Channel->BPQStream, EncodeCall(TNC->Channels[0]->MYCall));
	
	_strupr(TXBUFFERPTR);

	if (strstr(TXBUFFERPTR, "SWITCH") == 0)		// Not switch
	{
		char * Call, * Arg1;
		char * Context;
		char seps[] = " ,\r";

		Call = strtok_s(TXBUFFERPTR + 1, seps, &Context);
		Arg1 = strtok_s(NULL, seps, &Context);

		if (Arg1)
		{
			// Have a digi string

			// First digi is used as a port number. Any others are rwal digis or WINMOR/PACTOR

			if (Context[0])
				TNC->MSGLEN = sprintf(TXBUFFERPTR + 100, "C %s %s v %s\r", Arg1, Call, Context);
			else
				TNC->MSGLEN = sprintf(TXBUFFERPTR + 100, "C %s %s\r", Call, Arg1);
		}
		else
			TNC->MSGLEN = sprintf(TXBUFFERPTR + 100, "C %s\r", Call);

		strcpy(TXBUFFERPTR, TXBUFFERPTR + 100);

		SendMsg(Channel->BPQStream, TXBUFFERPTR, TNC->MSGLEN);
		
//		READCHANGE(Channel->BPQStream);			// Suppress Connected to Switch
		
		goto SENDHOSTOK;
		}
	}
		
	goto SENDHOSTOK;

DCMD:

//	DISCONNECT REQUEST

	Disconnect(Channel->BPQStream);

	goto SENDHOSTOK;

LCMD:

	PUTCHARx(TNC, TNC->MSGCHANNEL);		// REPLY ON SAME CHANNEL
	PUTCHARx(TNC, 1);

//	GET STATE AND QUEUED BUFFERS

	if (TNC->MSGCHANNEL)
		goto NORM_L;

//	TO MONITOR CHANNEL 

//	SEE IF MONITORED FRAMES AVAILABLE

	if (MONCount(TNC->Channels[0]->BPQStream))
		Work = 0x31;
	else
		Work = 0x30;

	goto MON_L;

NORM_L:

	LocalSessionState(Channel->BPQStream, &State, &Change, FALSE);

	if (State == 0)
		Work = '0';
	else
		Work = '4';					// AX.25 STATE

	PUTCHARx(TNC, Change + '0');	// Status Messages

	PUTCHARx(TNC, ' ');

//	GET OTHER QUEUE COUNTS

	Count = RXCount(Channel->BPQStream);

	sprintf(WorkString, "%d", Count);		// message count

	PUTSTRING(TNC, WorkString);
	PUTCHARx(TNC, ' ');

//	NOT SENT IS NUMBER ON OUR QUEUE, NOT ACKED NUMBER FROM SWITCH


//	SEE HOW MANY BUFFERS ATTACHED TO Q HEADER IN BX

 	Count = 0;// C_Q_COUNT(Channel->Chan_TXQ);

	sprintf(WorkString, "%d", Count);		// message count
	PUTSTRING(TNC, WorkString);
	PUTCHARx(TNC, ' ');

	if (Count > 8)
		Work = '8';				// Busy

	Count =  CountFramesQueuedOnSession(L4);

	sprintf(WorkString, "%d", Count);		// message count
	PUTSTRING(TNC, WorkString);
	PUTCHARx(TNC, ' ');

MON_L:

	PUTCHARx(TNC, '0');
	PUTCHARx(TNC, ' ');
	PUTCHARx(TNC, Work);	
	PUTCHARx(TNC, 0);

	return TRUE;

HOSTDATAPACKET:

//	}
//	{
//		UCHAR msg[100];

//	sprintf(msg,"Host Data Packet: Port %d\n", TNC->ComPort);
//	OutputDebugString(msg);
//	}
//	


//	IF WE ALREADY HAVE DATA QUEUED, ADD THIS IT QUEUE

//	if (Channel->Chan_TXQ)
//	{

//		//	COPY MESSAGE TO A CHAIN OF BUFFERS

//		if (QCOUNT < 10)
//			goto CANTSEND;		// NO SPACE - RETURN ERROR (?)

//QUEUEFRAME:

//	C_Q_ADD(Channel->Chan_TXQ, COPYMSGTOBUFFERS());		// RETURNS EDI = FIRST (OR ONLY) FRAGMENT

//	goto SENDHOSTOK;

	//	MAKE SURE NODE ISNT BUSY

	if (TNC->MSGCHANNEL == 0)		// UNPROTO Channel
		goto SendUnproto;

	Count =  CountFramesQueuedOnSession(L4);

//	if (Count > 4 || QCOUNT < 40)
//		goto QUEUEFRAME;

	//	OK TO PASS TO NODE
	
	SENDENFORCINGPACLEN(Channel, TNC->DEDTXBUFFER, TNC->MSGLENGTH);
		goto SENDHOSTOK;

SendUnproto:

	SendMsg(0, TXBUFFERPTR, TNC->MSGLENGTH);
		goto SENDHOSTOK;

POLL:

	PROCESSPOLL(TNC, Channel);
	return TRUE;

YCMD:

	*TNC->DEDCURSOR = 0;

	Work = atoi(&TXBUFFERPTR[1]);

	if (Work == 0)
		Work = 1;				// Mustn't release last stream

	if (Work >= 0 && Work <= MAXSTREAMS)
	{
		int Stream;

		if (Work < TNC->HOSTSTREAMS)
		{
			// Need to get rid of some streams

			for (i = Work + 1; i <= TNC->HOSTSTREAMS; i++)
			{
				Stream = TNC->Channels[i]->BPQStream;
			
				if (Stream)
				{
					Disconnect(Stream);
					READCHANGE(Stream);					// Prevent Initial *** disconnected

					DeallocateStream(Stream);

					Debugprintf("DED YCMD Release Stream %d", Stream);
				}

				free(TNC->Channels[i]);
				TNC->Channels[i] = 0;
			}
		}
		else
		{
			for (i = TNC->HOSTSTREAMS+1; i <= Work; i++)
			{
				AllocateDEDChannel(TNC, i);			// Also used by Y command handler
			}
		}
		TNC->HOSTSTREAMS = Work;
	}

	/*
	
	Why is this here?
	{
			int Len=0;
			UCHAR Message[1000];

			while (TNC->RXCOUNT > 0)
			{
				Message[Len++]= *(TNC->PUTPTR++);

				TNC->RXCOUNT--;

				if (TNC->PUTPTR == &TNC->RXBUFFER[TNCBUFFLEN])
					TNC->PUTPTR = (UCHAR *)&TNC->RXBUFFER;

				if (Len > 900) 
				{
					BPQSerialSendData(TNC, Message, Len);
					Len = 0;
				}
			}
				
			if (Len > 0) 
			{
				BPQSerialSendData(TNC, Message, Len);
			}
		}
	 _asm {

		 popad


	RET
*/
	
SENDHOSTOK:

	PUTCHARx(TNC, TNC->MSGCHANNEL);		// REPLY ON SAME CHANNEL
	PUTCHARx(TNC, 0);					// NOTHING DOING

	return TRUE;

}
PROCESSPOLL(struct TNCDATA * TNC, struct StreamInfo * Channel)
{
	int PollType;
	
	//	ASK SWITCH FOR STATUS CHANGE OR ANY RECEIVED DATA

	if (TNC->MSGLENGTH == 1)
		goto GENERALPOLL;

	PollType = 0;

//	HE'S BEING AWKWARD, AND USING SPECIFIC DATA/STATUS POLL

	 if (TNC->TXBUFFER[1] == '0')
		goto DATAONLY;

	 STATUSPOLL(TNC, Channel);

GENERALPOLL:

	if (STATUSPOLL(TNC, Channel))
		return TRUE;					// Status was reported

DATAONLY:

	if (DATAPOLL(TNC, Channel))
		return TRUE;					// Data Sent

	goto SENDHOSTOK;					// NOTHING DOING


SENDHOSTOK:

	PUTCHARx(TNC, TNC->MSGCHANNEL);		// REPLY ON SAME CHANNEL
	PUTCHARx(TNC, 0);					// NOTHING DOING

	return TRUE;
}

int ConvertToDEDMonFormat(struct TNCDATA * TNC, char * Decoded, int Len, MESSAGE * Rawdata)
{
	// convert tnc2 format monitor to ded format

	unsigned char * MONCURSOR=0;
	unsigned char MONHEADER[256];
	char * From, * To, * via, * ctl, *Context, *ptr, *iptr; 
	int pid, NR, NS, MonLen;
	char rest[20];

/*

    From DEDHOST Documentation
	
	  
Codes of 4 and 5 both signify a monitor header.  This  is  a  null-terminated
format message containing the

    fm {call} to {call} via {digipeaters} ctl {name} pid {hex}

string  that  forms  a monitor header.  The monitor header is also identical to
the monitor header displayed in user mode.  If the code was  4,  the  monitored
frame contained no information field, so the monitor header is all you get.  If
you monitor KB6C responding to a connect request from me and then poll  channel
0, you'll get:

    0004666D204B42364320746F204B42354D552063746C2055612070494420463000
    ! ! f m   K B 6 C   t o   K B 5 M U   c t l   U A   p i d   F 0 !
    ! !                                                             !
    ! +---- Code = 4 (Monitor, no info)        Null termination ----+
    +------- Channel = 0 (Monitor info is always on channel 0)

  If  the code was 5, the monitored frame did contain an information field.  In
this case, another G command to channel 0 will return the monitored information
with  a code of 6.  Since data transmissions must be transparent, the monitored
information is passed as a byte-count format transmission.    That  is,  it  is
preceded  by a count byte (one less than the number of bytes in the field).  No
null terminator is used in this case.  Since codes  4,  5,  and  6  pertain  to
monitored  information,  they will be seen only on channel 0.  If you hear KB6C
say "Hi" to NK6K, and then poll channel 0, you'll get:

    0005666D204B42364320746F204E4B364B2063746C204930302070494420463000
    ! ! f m   K B 6 C   t o   N K 6 K   c t l   I 0 0   p i d   F 0 !
    ! !                                           ! !               !
    ! !                           or whatever ----+-+               !
    ! !                                                             !
    ! +---- Code = 5 (Monitor, info follows)   Null termination ----+
    +------ Channel = 0 (Monitor info is always on channel 0)

and then the very next poll to channel 0 will get:

         00 06 02 48 69 0D
         !  !  !  H  i  CR
         !  !  !        !
         !  !  !        +----    (this is a data byte)
         !  !  +---- Count = 2   (three bytes of data)
         !  +------- Code = 6    (monitored information)
         +---------- Channel = 0 (Monitor info is always on channel 0)


*/

	iptr = strchr(&Decoded[10], ':');		// Info if present

	MONHEADER[0] = 4;					// NO DATA FOLLOWS
	MONCURSOR = &MONHEADER[1];

	if (strstr(Decoded, "NET/ROM") || strstr(Decoded, "NODES br") || strstr(Decoded, "INP3 RIF"))
		pid = 0xcf;
	else
		pid = 0xf0;

	From = strtok_s(&Decoded[10], ">", &Context);
	To = strtok_s(NULL, " ", &Context);

	via = strlop(To, ',');

	Context = strchr(Context, '<');
	if (Context == 0)
		return 0;

	ctl = strtok_s(NULL, ">", &Context);

	if (via)
		MONCURSOR += sprintf(MONCURSOR, "fm %s to %s via %s ctl ", From, To, via);
	else
		MONCURSOR += sprintf(MONCURSOR, "fm %s to %s ctl ", From, To);

	rest[0] = 0;

	switch (ctl[1])
	{
	case 'R':

		NR = ctl[strlen(ctl)-1] - 48;
		strlop(ctl, ' ');
		sprintf(rest, "%s%d", &ctl[1], NR);
		break;

	case 'I':

		ptr = strchr(ctl, 'S');
		if (ptr) NS = ptr[1] - 48;
		ptr = strchr(ctl, 'R');
		if (ptr) NR = ptr[1] - 48;
		sprintf(rest, "I%d%d pid %X", NS, NR, pid);

		if (pid == 0xcf)
		{
			// NETROM - pass the raw data
	
			MonLen = Rawdata->LENGTH - (MSGHDDRLEN + 16);	// Data portion of frame	
			memcpy(&TNC->MONBUFFER[2], &Rawdata->L2DATA[0], MonLen);

			MONHEADER[0] = 5;					// Data to follow
			TNC->MONFLAG = 1;					// Data to follow
			TNC->MONBUFFER[0] = 6;
			TNC->MONLENGTH = MonLen + 2;
			TNC->MONBUFFER[1] = (MonLen - 1);
		}
		else
		{		
			if (iptr)
			{
				iptr += 2;					// Skip colon and cr
				MonLen = Len - (iptr - Decoded);
				if (MonLen > 256)
					MonLen = 256;
		
				memcpy(&TNC->MONBUFFER[2], iptr, MonLen);


				if (MonLen == 0)				// No data
				{
					MONHEADER[0] = 4;			// No Data to follow
					TNC->MONFLAG = 0;			// No Data to follow
				}
				else
				{
					MONHEADER[0] = 5;					// Data to follow
					TNC->MONFLAG = 1;					// Data to follow
					TNC->MONBUFFER[0] = 6;
					TNC->MONLENGTH = MonLen + 2;
					TNC->MONBUFFER[1] = (MonLen - 1);
				}
			}
		}
		break;

	case 'C':

		strcpy(rest, "SABM");
		break;

	case 'D':

		if (ctl[1] == 'M')
			strcpy(rest, "DM");
		else
			strcpy(rest, "DISC");

		break;

	case 'U':

		if (ctl[2] == 'A')
			strcpy(rest, "UA");
		else
		{
			// UI

			int MonLen;;

			MONHEADER[0] = 5;					// Data to follow
			sprintf(rest, "UI pid %X", pid);
			TNC->MONFLAG = 1;					// Data to follow
			TNC->MONBUFFER[0] = 6;

			if (pid ==0xcf)
			{
				// NETROM - pass th raw data
	
				MonLen = Rawdata->LENGTH - (MSGHDDRLEN + 16);	// Data portion of frame	
				memcpy(&TNC->MONBUFFER[2], &Rawdata->L2DATA[0], MonLen);
			}
			else
			{
				ptr = strchr(Context, ':');

				if (ptr == 0)
				{
					TNC->MONFLAG = 0;
					return 0;
				}

				ptr += 2;					// Skip colon and cr
				MonLen = Len - (ptr - Decoded);
				memcpy(&TNC->MONBUFFER[2], ptr, MonLen);
			}

			if (MonLen == 0)				// No data
			{
				MONHEADER[0] = 4;			// No Data to follow
				TNC->MONFLAG = 0;			// No Data to follow
			}
			else
			{
				TNC->MONLENGTH = MonLen + 2;
				TNC->MONBUFFER[1] = (MonLen - 1);
			}
			break;
		}

	default:
		rest[0] = 0;
	}

	MONCURSOR += sprintf(MONCURSOR, "%s", rest);

	if (MONCURSOR == &MONHEADER[1])
		return 0;					// NOTHING DOING

	*MONCURSOR++ = 0;				// NULL TERMINATOR

	SENDCMDREPLY(TNC, MONHEADER, MONCURSOR - &MONHEADER[0]);
	return 1;
}

//	GET THE CONTROL BYTE, TO SEE IF THIS FRAME IS TO BE DISPLAYED
/*

static char CTL_MSG[]=" ctl ";
static char VIA_MSG[]=" via ";
static char PID_MSG[]=" pid ";
static char SABM_MSG[]="SABM";
static char DISC_MSG[]="DISC";
static char UA_MSG[]="UA";

static char DM_MSG	[]="DM";
static char RR_MSG	[]="RR";
static char RNR_MSG[]="RNR";
static char I_MSG[]="I pid ";
static char UI_MSG[]="UI pid ";
static char FRMR_MSG[]="FRMR";
static char REJ_MSG[]="REJ";

	PUSH	EDI
	MOV	ECX,8			; MAX DIGIS
CTRLLOOP:
	TEST	BYTE PTR (MSGCONTROL-1)[EDI],1
	JNZ	CTRLFOUND

	ADD	EDI,7
	LOOP	CTRLLOOP
;
;	INVALID FRAME
;
	POP	EDI
	RET

CTRLFOUND:
	MOV	AL,MSGCONTROL[EDI]

	and	AL,NOT PFBIT		; Remove P/F bit
	mov	FRAME_TYPE,AL

	
	POP	EDI
;
	TEST	AL,1			; I FRAME
	JZ	IFRAME

	CMP	AL,3			; UI
	JE	OKTOTRACE		; ALWAYS DO UI

	CMP	AL,FRMR
	JE	OKTOTRACE		; ALWAYS DO FRMR
;
;	USEQ/CONTROL - TRACE IF MCOM ON
;
	CMP	MCOM,0
	JNE	OKTOTRACE

	RET

;-----------------------------------------------------------------------------;
;       Check for MALL                                                        ;
;-----------------------------------------------------------------------------;

IFRAME:
	cmp	MALL,0
	jne	OKTOTRACE

	ret

OKTOTRACE:
;
;-----------------------------------------------------------------------------;
;       Get the port number of the received frame                             ;
;-----------------------------------------------------------------------------;
;
;	CHECK FOR PORT SELECTIVE MONITORING
;
	MOV	CL,MSGPORT[EDI]
	mov	PORT_NO,CL

	DEC	CL
	MOV	EAX,1
	SHL	EAX,CL			; SHIFT BIT UP

	TEST	MMASK,EAX
	JNZ	TRACEOK1

	RET

TRACEOK1:

	MOV	FRMRFLAG,0
	push	EDI
	mov	AH,MSGDEST+6[EDI]
	mov	AL,MSGORIGIN+6[EDI]

;
;       Display Origin Callsign                                               ;
;

;    0004666D204B42364320746F204B42354D552063746C2055612070494420463000
;    ! ! f m   K B 6 C   t o   K B 5 M U   c t l   U A   p i d   F 0 !
;    ! !                                                             !
;    ! +---- Code = 4 (Monitor, no info)        Null termination ----+
 ;   +------- Channel = 0 (Monitor info is always on channel 0)

	mov	ESI,OFFSET FM_MSG
	call	NORMSTR

	lea	ESI,MSGORIGIN[EDI]
	call	CONVFROMAX25
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	pop	EDI
	push	EDI

	mov	ESI,OFFSET TO_MSG
	call	NORMSTR
;
;       Display Destination Callsign                                          ;
;
	lea	ESI,MSGDEST[EDI]
	call	CONVFROMAX25
	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	pop	EDI
	push	EDI

	mov	AX,MMSGLENGTH[EDI]
	mov	FRAME_LENGTH,AX
	mov	ECX,8			; Max number of digi-peaters
;
;       Display any Digi-Peaters                                              ;
;
	test	MSGORIGIN+6[EDI],1
	jnz	NO_MORE_DIGIS

	mov	ESI,OFFSET VIA_MSG
	call	NORMSTR
	jmp short skipspace

NEXT_DIGI:
	test	MSGORIGIN+6[EDI],1
	jnz	NO_MORE_DIGIS

	mov	AL,' '
	call	MONPUTCHAR
skipspace:
	add	EDI,7
	sub	FRAME_LENGTH,7		; Reduce length

	push	EDI
	push	ECX
	lea	ESI,MSGORIGIN[EDI]
	call	CONVFROMAX25		; Convert to call

	push	EAX			; Last byte is in AH

	mov	ESI,OFFSET NORMCALL
	call	DISPADDR

	pop	EAX

	test	AH,80H
	jz	NOT_REPEATED

	mov	AL,'*'
	call	MONPUTCHAR

NOT_REPEATED:
	pop	ECX
	pop	EDI
	loop	NEXT_DIGI

NO_MORE_DIGIS:	

;----------------------------------------------------------------------------;
;       Display ctl                                    ;
;----------------------------------------------------------------------------;

	mov	ESI,OFFSET CTL_MSG
	call	NORMSTR

;-----------------------------------------------------------------------------;
;       Start displaying the frame information                                ;
;-----------------------------------------------------------------------------;


	mov	INFO_FLAG,0

	mov	AL,FRAME_TYPE

	test	AL,1
	jne	NOT_I_FRAME

;-----------------------------------------------------------------------------;
;       Information frame                                                     ;
;-----------------------------------------------------------------------------;

	mov	AL,'I'
	call	MONPUTCHAR
	mov	INFO_FLAG,1

	mov	ESI,OFFSET I_MSG
	call	NORMSTR

	lea	ESI,MSGPID[EDI]
	lodsb

	call BYTE_TO_HEX
	

	jmp	END_OF_TYPE

NOT_I_FRAME:

;-----------------------------------------------------------------------------;
;       Un-numbered Information Frame                                         ;
;-----------------------------------------------------------------------------;

	cmp	AL,UI
	jne	NOT_UI_FRAME

	mov	ESI,OFFSET UI_MSG
	call	NORMSTR

	lea	ESI,MSGPID[EDI]
	lodsb

	call BYTE_TO_HEX
	
	mov	INFO_FLAG,1
	jmp	END_OF_TYPE

NOT_UI_FRAME:
	test	AL,10B
	jne	NOT_R_FRAME

;-----------------------------------------------------------------------------;
;       Process supervisory frames                                            ;
;-----------------------------------------------------------------------------;


	and	AL,0FH			; Mask the interesting bits
	cmp	AL,RR	
	jne	NOT_RR_FRAME

	mov	ESI,OFFSET RR_MSG
	call	NORMSTR
	jmp	END_OF_TYPE

NOT_RR_FRAME:
	cmp	AL,RNR
	jne	NOT_RNR_FRAME

	mov	ESI,OFFSET RNR_MSG
	call	NORMSTR
	jmp END_OF_TYPE

NOT_RNR_FRAME:
	cmp	AL,REJ
	jne	NOT_REJ_FRAME

	mov	ESI,OFFSET REJ_MSG
	call	NORMSTR
	jmp	SHORT END_OF_TYPE

NOT_REJ_FRAME:
	mov	AL,'?'			; Print "?"
	call	MONPUTCHAR
	jmp	SHORT END_OF_TYPE

;
;       Process all other frame types                                         ;
;

NOT_R_FRAME:
	cmp	AL,UA
	jne	NOT_UA_FRAME

	mov	ESI,OFFSET UA_MSG
	call	NORMSTR
	jmp	SHORT END_OF_TYPE

NOT_UA_FRAME:
	cmp	AL,DM
	jne	NOT_DM_FRAME

	mov	ESI,OFFSET DM_MSG
	call	NORMSTR
	jmp	SHORT END_OF_TYPE

NOT_DM_FRAME:
	cmp	AL,SABM
	jne	NOT_SABM_FRAME

	mov	ESI,OFFSET SABM_MSG
	call	NORMSTR
	jmp	SHORT END_OF_TYPE

NOT_SABM_FRAME:
	cmp	AL,DISC
	jne	NOT_DISC_FRAME

	mov	ESI,OFFSET DISC_MSG
	call	NORMSTR
	jmp	SHORT END_OF_TYPE

NOT_DISC_FRAME:
	cmp	AL,FRMR
	jne	NOT_FRMR_FRAME

	mov	ESI,OFFSET FRMR_MSG
	call	NORMSTR
	MOV	FRMRFLAG,1
	jmp	SHORT END_OF_TYPE

NOT_FRMR_FRAME:
	mov	AL,'?'
	call	MONPUTCHAR

END_OF_TYPE:

	CMP	FRMRFLAG,0
	JE	NOTFRMR
;
;	DISPLAY FRMR BYTES
;
	lea	ESI,MSGPID[EDI]
	MOV	CX,3			; TESTING
FRMRLOOP:
	lodsb
	CALL	BYTE_TO_HEX

	LOOP	FRMRLOOP

	JMP	NO_INFO

NOTFRMR:

	MOVZX	ECX,FRAME_LENGTH


	cmp	INFO_FLAG,1		; Is it an information packet ?
	jne	NO_INFO


	XOR	AL,AL			; IN CASE EMPTY

	sub	ECX,23
	CMP ecx,0
	je	NO_INFO			; EMPTY I FRAME

;
;	PUT DATA IN MONBUFFER, LENGTH IN MONLENGTH
;

	pushad
}
	TNC->MONFLAG = 1;

	_asm{

	popad

	MOV	MONHEADER,5		; DATA FOLLOWS

	cmp	ECX,257
	jb	LENGTH_OK
;
	mov	ECX,256
;
LENGTH_OK:
;
	mov	MonDataLen, ECX

	pushad

	}

	TNC->MONBUFFER[1] = MonDataLen & 0xff;
	TNC->MONBUFFER[1]--;


	TNC->MONLENGTH = MonDataLen+2;

	ptr1=&TNC->MONBUFFER[2];

	_asm{

	popad
		
	MOV	EDI,ptr1

MONCOPY:
	LODSB
	CMP	AL,7			; REMOVE BELL
	JNE	MONC00

	MOV	AL,20H
MONC00:
	STOSB

	LOOP	MONCOPY

	POP	EDI
	RET

NO_INFO:
;
;	ADD CR UNLESS DATA ALREADY HAS ONE
;
	CMP	AL,CR
	JE	NOTANOTHER

	mov	AL,CR
	call	MONPUTCHAR

NOTANOTHER:
;
	pop	EDI
	ret

;----------------------------------------------------------------------------;
;       Display ASCIIZ strings                                               ;
;----------------------------------------------------------------------------;

NORMSTR:
	lodsb
	cmp	AL,0		; End of String ?
	je	NORMSTR_RET	; Yes
	call	MONPUTCHAR
	jmp	SHORT NORMSTR

NORMSTR_RET:
	ret

;-----------------------------------------------------------------------------;
;       Display Callsign pointed to by SI                                     ;
;-----------------------------------------------------------------------------;

DISPADDR:
	jcxz	DISPADDR_RET

	lodsb
	call	MONPUTCHAR

	loop	DISPADDR

DISPADDR_RET:
	ret


;-----------------------------------------------------------------------------;
;       Convert byte in AL to nn format                                       ;
;-----------------------------------------------------------------------------;

DISPLAY_BYTE_2:
	cmp	AL,100
	jb	TENS_2

	sub	AL,100
	jmp	SHORT DISPLAY_BYTE_2

TENS_2:
	mov	AH,0

TENS_LOOP_2:
	cmp	AL,10
	jb	TENS_LOOP_END_2

	sub	AL,10
	inc	AH
	jmp	SHORT TENS_LOOP_2

TENS_LOOP_END_2:
	push	EAX
	mov	AL,AH
	add	AL,30H
	call	MONPUTCHAR
	pop	EAX

	add	AL,30H
	call	MONPUTCHAR

	ret

;-----------------------------------------------------------------------------;
;       Convert byte in AL to Hex display                                     ;
;-----------------------------------------------------------------------------;

BYTE_TO_HEX:
	push	EAX
	shr	AL,1
	shr	AL,1
	shr	AL,1
	shr	AL,1
	call	NIBBLE_TO_HEX
	pop	EAX
	call	NIBBLE_TO_HEX
	ret

NIBBLE_TO_HEX:
	and	AL,0FH
	cmp	AL,10

	jb	LESS_THAN_10
	add	AL,7

LESS_THAN_10:
	add	AL,30H
	call	MONPUTCHAR
	ret



CONVFROMAX25:
;
;	CONVERT AX25 FORMAT CALL IN [SI] TO NORMAL FORMAT IN NORMCALL
;	   RETURNS LENGTH IN CX AND NZ IF LAST ADDRESS BIT IS SET
;
	PUSH	ESI			; SAVE
	MOV	EDI,OFFSET NORMCALL
	MOV	ECX,10			; MAX ALPHANUMERICS
	MOV	AL,20H
	REP STOSB			; CLEAR IN CASE SHORT CALL
	MOV	EDI,OFFSET NORMCALL
	MOV	CL,6
CONVAX50:
	LODSB
	CMP	AL,40H
	JE	CONVAX60		; END IF CALL - DO SSID

	SHR	AL,1
	STOSB
	LOOP	CONVAX50
CONVAX60:
	POP	ESI
	ADD	ESI,6			; TO SSID
	LODSB
	MOV	AH,AL			; SAVE FOR LAST BIT TEST
	SHR	AL,1
	AND	AL,0FH
	JZ	CONVAX90		; NO SSID - FINISHED
;
	MOV	BYTE PTR [EDI],'-'
	INC	EDI
	CMP	AL,10
	JB	CONVAX70
	SUB	AL,10
	MOV	BYTE PTR [EDI],'1'
	INC	EDI
CONVAX70:
	ADD	AL,30H			; CONVERT TO DIGIT
	STOSB
CONVAX90:
	MOV	ECX,EDI
	SUB	ECX,OFFSET NORMCALL
	MOV	NORMLEN,ECX		; SIGNIFICANT LENGTH

	TEST	AH,1			; LAST BIT SET?
	RET


PUTCHAR:

	pushad
	push eax
	push TNC
	call PUTCHARx
	pop eax
	pop eax
	popad
	ret
}
}
*/



VOID SENDENFORCINGPACLEN(struct StreamInfo * Channel, char * Msg, int Len)
{		
	int Paclen = 0;
	
	if (Len == 0)
		return;
	
	if (BPQHOSTVECTOR[Channel->BPQStream-1].HOSTSESSION)
		Paclen = BPQHOSTVECTOR[Channel->BPQStream-1].HOSTSESSION->SESSPACLEN;

	if (Paclen == 0)
		goto nochange;						// paclen not set

fragloop:

	if (Len <= Paclen)
		goto nochange;						// msglen <= paclen

//	need to fragment

	SendMsg(Channel->BPQStream, Msg, Paclen);

	Msg += Paclen;

	Len -= Paclen;

	if (Len)
		goto fragloop;

	return;

nochange:

	SendMsg(Channel->BPQStream, Msg, Len);
	return;
}

int DOCOMMAND(struct TNCDATA * conn)
{
	char Errbuff[500];
	int i;

//	PROCESS NORMAL MODE COMMAND

	sprintf(Errbuff, "BPQHOST Port %d Normal Mode CMD %s\n",conn->ComPort, conn->TXBUFFER);  
	OutputDebugString(Errbuff);
 
//	IF ECHO ENABLED, ECHO IT

	if (conn->ECHOFLAG)
	{
		UCHAR * ptr = conn->TXBUFFER;
		UCHAR c;

		do 
		{
			c = *(ptr++);
			
			if (c == 0x1b) c = ':';

			PUTCHARx(conn, c);

		} while (c != 13);
	}

	if (conn->TXBUFFER[0] != 0x1b)
		goto NOTCOMMAND;		// DATA IN NORMAL MODE - IGNORE

	switch (toupper(conn->TXBUFFER[1]))
	{	
	case 'J':

		if (conn->TXBUFFER[6] == 0x0d)
			conn->MODE = 0;
		else
			conn->MODE = conn->TXBUFFER[6] & 1;

		if (conn->MODE)
			EnableAppl(conn);
		else
			DisableAppl(conn);

		if (conn->MODE)
		{
			//	send host mode ack

//			PUTCHARx(conn, 0);
//			PUTCHARx(conn, 0);

			conn->CURSOR = (UCHAR *)&conn->TXBUFFER;
			return 0;
		}

		break;

	case 'E':

		conn->ECHOFLAG = conn->TXBUFFER[2] & 1;
		break;

	case 'I':
	{
		// Save call 

		char * Call = &conn->TXBUFFER[2];
		
		*(conn->CURSOR - 2) = 0;

		for (i = 0; i <= conn->HOSTSTREAMS; i++)
		{
			strcpy(conn->Channels[i]->MYCall, Call);
		}

		break;;
	}
	case 'P':

//	PARAMS COMMAND - RETURN FIXED STRING

		PARAMPORT = conn->TXBUFFER[2];

		for (i=0; i < LPARAMREPLY; i++)
		{
			PUTCHARx(conn, PARAMREPLY[i]);
		}

		break;

	case 'S':
	case 'D':

		// Return Channel Not Connected

		PUTSTRING(conn, "* CHANNEL NOT CONNECTED *\r");

	default:

		break;

	}

//	PUTCHARx(conn, 'c');
//	PUTCHARx(conn, 'm');
//	PUTCHARx(conn, 'd');
//	PUTCHARx(conn, ':');
//	PUTCHARx(conn, 13);

NOTCOMMAND:

	conn->CURSOR = (UCHAR *)&conn->TXBUFFER;

	return 0;

}

VOID SENDCMDREPLY(struct TNCDATA * TNC, char * Msg, int Len)
{
	int n;
	
	if (Len == 0)
		return;

	PUTCHARx(TNC, TNC->MSGCHANNEL);

	for (n = 0; n < Len; n++)
	{
		PUTCHARx(TNC, Msg[n]);
	}
}

int STATUSPOLL(struct TNCDATA * TNC, struct StreamInfo * Channel)
{
	// Status Poll

	int State, Change, i;
	char WorkString[256];
	
	 if (TNC->MSGCHANNEL == 0)		// Monitor Chan
		return 0;
	
	LocalSessionState(Channel->BPQStream, &State, &Change, TRUE);

	if (Change == 0)
		return 0;

	//	PORT HAS CONNECTED OR DISCONNECTED - SEND STATUS CHANGE TO PC

	if (State == 0)
	{
		//	DISCONNECTED

		i = sprintf(CONMSG, "\x3(%d) DISCONNECTED fm 0:SWITCH\r", TNC->MSGCHANNEL);
		i++;
	}
	else
	{
		//	GET CALLSIGN
	
		GetCallsign(Channel->BPQStream, WorkString);
		strlop(WorkString, ' ');
		i = sprintf(CONMSG, "\x3(%d) CONNECTED to %s\r", TNC->MSGCHANNEL, WorkString);
		i++;
	}

	SENDCMDREPLY(TNC, CONMSG, i);
	return 1;
}

int DATAPOLL(struct TNCDATA * TNC, struct StreamInfo * Channel)
{
	unsigned char NODEBUFFER[300];		// MESSAGE FROM NODE
	int Len, Count, i;
	time_t stamp;
	char * ptr1;

	if (TNC->MSGCHANNEL == 0)
	{
		//	POLL FOR MONITOR DATA
	 
		if (TNC->MONFLAG == 0)
			goto NOMONITOR;

		//	HAVE ALREADY GOT DATA PART OF MON FRAME OT SEND

		TNC->MONFLAG = 0;

		ptr1 = (UCHAR *)&TNC->MONBUFFER;

		if (TNC->MONLENGTH)
		{
			SENDCMDREPLY(TNC, ptr1, TNC->MONLENGTH);
			return TRUE;
		}
		
		OutputDebugString("BPQHOST Mondata Flag Set with no data");

NOMONITOR:

	//	SEE IF ANYTHING TO MONITOR

		stamp = GetRaw(TNC->Channels[0]->BPQStream, (char *)&MONITORDATA, &Len, &Count);

		if (Len)
		{
			// Use Normal Decode, then reformat to DED standard

			ULONG SaveMMASK = MMASK;
			BOOL SaveMTX = MTX;
			BOOL SaveMCOM = MCOM;
			BOOL SaveMUI = MUIONLY;
			unsigned char Decoded[1000];

			SetTraceOptionsEx(TNC->MMASK, TNC->MTX, TNC->MCOM, 0);
			Len = IntDecodeFrame(&MONITORDATA, Decoded, (UINT)stamp, TNC->MMASK, FALSE, FALSE);
			SetTraceOptionsEx(SaveMMASK, SaveMTX, SaveMCOM, SaveMUI);

			if (Len)	
			{
				return ConvertToDEDMonFormat(TNC, Decoded, Len, &MONITORDATA);
			}
		}			
		return 0;
	}

	// Look for session data

	GetMsg(Channel->BPQStream, NODEBUFFER, &Len, &Count);

	if (Len == 0)
		return 0;

	if (Len > 256)
	{
		Debugprintf("BPQHOST Corrupt Length = %d", Len);
		return 0;
	}

	//	SEND DATA

	// If a failure, set a close timer (for Airmail, etc)

	NODEBUFFER[Len] = 0;	// For strstr

	if (strstr(NODEBUFFER, "} Downlink connect needs port number") ||
			strstr(NODEBUFFER, "} Error - TNC Not Ready") ||
			strstr(NODEBUFFER, "} Failure with ") ||
			strstr(NODEBUFFER, "} Sorry, "))
		Channel->CloseTimer = CloseDelay * 10;
	else
		Channel->CloseTimer = 0;			// Cancel Timer
	
	PUTCHARx(TNC, TNC->MSGCHANNEL);		// REPLY ON SAME CHANNEL
	PUTCHARx(TNC, 7);
	PUTCHARx(TNC, Len - 1);
	
	for (i = 0; i < Len; i++)
	{
		PUTCHARx(TNC, NODEBUFFER[i]);
	}
	
	return 1;				// HAVE SEND SOMETHING
}









// Kantronics Host Mode Stuff

// Kantronics Host Mode Stuff

#define	FEND	0xC0	// KISS CONTROL CODES 
#define	FESC	0xDB
#define	TFEND	0xDC
#define	TFESC	0xDD


static VOID ProcessKHOSTPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Len);
VOID ProcessKNormCommand(struct TNCDATA * conn, UCHAR * rxbuffer);
VOID SendKISSData(struct TNCDATA * conn, UCHAR * txbuffer, int Len);
static int	KissDecode(UCHAR * inbuff, UCHAR * outbuff, int len);
static int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len);
static int DoReceivedData(struct TNCDATA * conn, struct StreamInfo * channel);



VOID ProcessPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Len)
{
	UCHAR * FendPtr;
	int NewLen;

	if (!conn->MODE)
	{
		//	In Terminal Mode - Pass to Term Mode Handler
		
		ProcessKPacket(conn, rxbuffer, Len);
		conn->RXBPtr = 0;
		return;
	}

	//	Split into KISS Packets. By far the most likely is a single KISS frame
	//	so treat as special case

	if (!(rxbuffer[0] == FEND))
	{
		// Getting Non Host Data in Host Mode - Appl will have to sort the mess
		// Discard any data

		conn->RXBPtr = 0;
		return;
	}

	conn->RXBPtr = 0;				// Assume we will use all data in buffer - will reset if part packet received
	
	FendPtr = memchr(&rxbuffer[1], FEND, Len-1);
	
	if (FendPtr == &rxbuffer[Len-1])
	{
		ProcessKHOSTPacket(conn, &rxbuffer[1], Len - 2);
		return;
	}

	if (FendPtr == NULL)
	{
		// We have a partial Packet - Save it

		conn->RXBPtr = Len;
		memcpy(&conn->RXBUFFER[0], rxbuffer, Len);
		return;
	}
		
	// Process the first Packet in the buffer

	NewLen =  FendPtr - rxbuffer -1;
	ProcessKHOSTPacket(conn, &rxbuffer[1], NewLen );
	
	// Loop Back

	ProcessPacket(conn, FendPtr+1, Len - NewLen -2);
	return;

}

VOID ProcessKPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Len)
{
	UCHAR Char;
	UCHAR * cmdStart;
	int cmdlen = 0;

	// we will often get a whole connamd at once, but may not, so be prepared to receive char by char
	//	Could also get more than one command per packet

	cmdStart = rxbuffer;

	if (rxbuffer[0] == FEND && rxbuffer[Len-1] == FEND)
	{
		// Term thinks it is hosr mode

		// Unless it is FEND FF FEND (exit KISS) or FEND Q FEND (exit host)

		if (rxbuffer[2] == FEND)
		{
			if (rxbuffer[1] == 255 || rxbuffer[1] == 'q')
			{
				// If any more , process it.
				
				if (Len == 3)
					return;

				Len -= 3;
				rxbuffer+= 3;
				ProcessKPacket(conn, rxbuffer, Len);
				return;
			}
		}
		conn->MODE = 1;
		return;
	}
	
	while (Len > 0)
	{
		Char = *(rxbuffer++);
		Len--;
		cmdlen++;

//		if (conn->TermPtr > 120) conn->TermPtr = 120;	// Prevent overflow 

		if (conn->ECHOFLAG) BPQSerialSendData(conn, &Char, 1);

		if (Char == 0x0d)
		{
			// We have a command line
		
			*(rxbuffer-1) = 0;
			ProcessKNormCommand(conn, cmdStart);
			conn->RXBPtr -= cmdlen;
			cmdlen = 0;
			cmdStart = rxbuffer;
		}
	}
}

VOID ProcessKNormCommand(struct TNCDATA * conn, UCHAR * rxbuffer)
{
//	UCHAR CmdReply[]="C00";
	UCHAR ResetReply[] = "\xC0\xC0S00\xC0";
	int Len;

	char seps[] = " \t\r";
	char * Command, * Arg1, * Arg2;
	char * Context;

	if (conn->Channels[1]->Connected)
	{
		Len = strlen(rxbuffer);
		rxbuffer[Len] = 0x0d;
		SendMsg(conn->Channels[1]->BPQStream, rxbuffer, Len+1);
		return;
	}

    Command = strtok_s(rxbuffer, seps, &Context);
    Arg1 = strtok_s(NULL, seps, &Context);
    Arg2 = strtok_s(NULL, seps, &Context);

	if (Command == NULL)
	{
		BPQSerialSendData(conn, "cmd:", 4);
		return;
	}
		
	if (_stricmp(Command, "RESET") == 0)
	{
		if (conn->nextMode)		
			BPQSerialSendData(conn, ResetReply, 6);
		else
			BPQSerialSendData(conn, "cmd:", 4);

		conn->MODE = conn->nextMode;

		if (conn->MODE)
			EnableAppl(conn);
		else
			DisableAppl(conn);

		return;
	}

	if (_stricmp(Command, "K") == 0)
	{
		SessionControl(conn->Channels[1]->BPQStream, 1, 0);
		return;
	}

	if (_memicmp(Command, "IN", 2) == 0)
	{
		if (Arg1)
		{
			if (_stricmp(Arg1, "HOST") == 0)
				conn->nextMode = TRUE;
			else
				conn->nextMode = FALSE;
		}

		BPQSerialSendData(conn, "INTFACE was TERMINAL\r", 21);
		BPQSerialSendData(conn, "cmd:", 4);
		return;
	}

//cmd:

//INTFACE HOST
//INTFACE was TERMINAL
//cmd:RESET
//응S00�
//픀20XFLOW OFF�


	//SendKISSData(conn, CmdReply, 3);
	
	BPQSerialSendData(conn, "cmd:", 4);


	//	Process Non-Hostmode Packet

	return;
}
int FreeBytes = 999;

static VOID ProcessKHOSTPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Len)
{
	struct StreamInfo * channel;
	UCHAR Command[80];
	UCHAR Reply[400];
	UCHAR TXBuff[400];
	UCHAR CmdReply[]="C00";

	UCHAR Chan, Stream;
	int i, j, TXLen, StreamNo;
	
	char * Cmd, * Arg1, * Arg2, * Arg3;
	char * Context;
	char seps[] = " \t\r\xC0";
	int CmdLen;

	if ((Len == 1) && ((rxbuffer[0] == 'q') || (rxbuffer[0] == 'Q')))
	{
		// Force Back to Command Mode

		Sleep(3000);
		conn->MODE = FALSE;
		BPQSerialSendData(conn, "\r\r\rcmd:", 7);
		return;
	}

	if (rxbuffer[0] == '?')
	{
		// what is this ???

		memcpy(Reply,CmdReply,3);
		SendKISSData(conn, Reply, 3);
		return;
	}

	Chan = rxbuffer[1];
	Stream = rxbuffer[2];

	StreamNo = Stream == '0' ? 0 : Stream - '@';

	if (StreamNo > conn->HOSTSTREAMS)
	{
		SendKISSData(conn, "C00Invalid Stream", 17);
		return;		
	}

	switch (rxbuffer[0])
	{
	case 'C':

		// Command Packet. Extract Command

		if (Len > 80) Len = 80;
	
		memcpy(Command, &rxbuffer[3], Len-3);
		Command[Len-3] = 0;

		Cmd = strtok_s(Command, seps, &Context);
		Arg1 = strtok_s(NULL, seps, &Context);
		Arg2 = strtok_s(NULL, seps, &Context);
		Arg3 = strtok_s(NULL, seps, &Context);
		CmdLen = strlen(Cmd);

		if (_stricmp(Cmd, "S") == 0)
		{
			// Status

			FreeBytes = 2000;

			// Ideally I should do flow control by channel, but Paclink (at least) doesn't have a mechanism

			for (i = 1; i < conn->HOSTSTREAMS; i++)
			{
				if (conn->Channels[i]->Connected)
					if (TXCount(conn->Channels[1]->BPQStream) > 10)
						FreeBytes = 0;
			}

			// This format works with Paclink and RMS Packet, but it doesn't seem to conform to the spec

			// I think maybe the Channel status should be in the same Frame.

			TXLen = sprintf(Reply, "C00FREE BYTES %d\r", FreeBytes);
			SendKISSData(conn, Reply, TXLen);

			for (j=1; j <= conn->HOSTSTREAMS; j++)
			{
				channel = conn->Channels[j];
			
				if (channel->Connected)
				{
					TXLen = sprintf(Reply, "C00%c/V stream - CONNECTED to %s", j + '@', "SWITCH");
					SendKISSData(conn, Reply, TXLen);
				}
//				else
//					TXLen = sprintf(Reply, "C00%c/V stream - DISCONNECTED", j + '@');

			}
			return;
		}

		if (_memicmp(Cmd, "C", CmdLen) == 0)
		{
			int Port;
			struct StreamInfo * channel;
			int BPQStream;
			UCHAR * MYCall;

			// Connect. If command has a via string and first call is numeric use it as a port number

			if (StreamNo == 0)
			{
				Stream = 'A';
				StreamNo = 1;
			}

			if (Arg2 && Arg3)	
			{
				if (_memicmp(Arg2, "via", strlen(Arg2)) == 0)
				{
					// Have a via string as 2nd param 

					Port = atoi(Arg3);
					{
						if (Port > 0)					// First Call is numeric
						{
							if (strlen(Context) > 0)	// More Digis
								TXLen = sprintf(TXBuff, "c %s %s v %s\r", Arg3, Arg1, Context);
							else
								TXLen = sprintf(TXBuff, "c %s %s\r", Arg3, Arg1);
						}
						else
						{
							// First Call Isn't Numeric. This won't work, as Digis without a port is invalid
							
							SendKISSData(conn, "C00Invalid via String (First must be Port)", 42);
							return;		

						}
					}
				}
				else
					TXLen = sprintf(TXBuff, "%s %s %s %s %s\r", Cmd, Arg1, Arg2, Arg3, Context);

			}
			else
			{
				TXLen = sprintf(TXBuff, "C %s\r", Arg1);
			}

			Reply[0] = 'C';
			Reply[1] = Chan;
			Reply[2] = Stream;
			SendKISSData(conn, Reply, 3);

			channel = conn->Channels[StreamNo];
			BPQStream = channel->BPQStream;
			MYCall = (UCHAR *)&channel->MYCall;

			Connect(BPQStream);

			if (MYCall[0] > 0)
			{
				ChangeSessionCallsign(BPQStream, EncodeCall(MYCall));
			}

			SendMsg(conn->Channels[StreamNo]->BPQStream, TXBuff, TXLen);

			return;
			
		}

		if (_stricmp(Cmd, "D") == 0)
		{
			// Disconnect

			if (StreamNo == 0)
			{
				Stream = 'A';
				StreamNo = 1;
			}

			SessionControl(conn->Channels[StreamNo]->BPQStream, 2, 0);
			return;
		}

		if (_memicmp(Cmd, "INT", 3) == 0)
		{
			SendKISSData(conn, "C00INTFACE HOST", 15);
			return;
		}

		if (_stricmp(Cmd, "PACLEN") == 0)
		{
			SendKISSData(conn, "C00PACLEN 128/128", 17);
			return;
		}

		if (_memicmp(Cmd, "MYCALL", CmdLen > 1 ? CmdLen : 2) == 0)
		{
			if (strlen(Arg1) < 30)
				strcpy(conn->Channels[StreamNo]->MYCall, Arg1);
		}

		memcpy(Reply,CmdReply,3);
		SendKISSData(conn, Reply, 3);
		return;

	case 'D':

		// Data to send

			
		if (StreamNo > conn->HOSTSTREAMS) return;		// Protect

		TXLen = KissDecode(&rxbuffer[3], TXBuff, Len-3);
		SendMsg(conn->Channels[StreamNo]->BPQStream, TXBuff, TXLen);

		conn->Channels[StreamNo]->CloseTimer = 0;			// Cancel Timer

		return;

	default:

		memcpy(Reply,CmdReply,3);
		SendKISSData(conn, Reply, 3);
		return;
	}
}

static int KissDecode(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i,txptr=0;
	UCHAR c;

	for (i=0;i<len;i++)
	{
		c=inbuff[i];

		if (c == FESC)
		{
			c=inbuff[++i];
			{
				if (c == TFESC)
					c=FESC;
				else
				if (c == TFEND)
					c=FEND;
			}
		}

		outbuff[txptr++]=c;
	}

	return txptr;

}

VOID SendKISSData(struct TNCDATA * conn, UCHAR * txbuffer, int Len)
{
	// Send A Packet With KISS Encoding

	UCHAR EncodedReply[800];
	int TXLen;

	TXLen = KissEncode(txbuffer, EncodedReply, Len);

	BPQSerialSendData(conn, EncodedReply, TXLen);

}

static int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len)
{
	int i,txptr=0;
	UCHAR c;

	outbuff[0]=FEND;
	txptr=1;

	for (i=0;i<len;i++)
	{
		c=inbuff[i];
		
		switch (c)
		{
		case FEND:
			outbuff[txptr++]=FESC;
			outbuff[txptr++]=TFEND;
			break;

		case FESC:

			outbuff[txptr++]=FESC;
			outbuff[txptr++]=TFESC;
			break;

		default:

			outbuff[txptr++]=c;
		}
	}

	outbuff[txptr++]=FEND;

	return txptr;

}

int KANTConnected(struct TNCDATA * conn, struct StreamInfo * channel, int Stream)
{
	byte ConnectingCall[10];
	UCHAR Msg[50];
	int Len;

	GetCallsign(channel->BPQStream, ConnectingCall);
	strlop(ConnectingCall, ' ');

	if (conn->MODE)
	{
		Len = sprintf (Msg, "S1%c*** CONNECTED to %s ", Stream + '@', ConnectingCall);
		SendKISSData(conn, Msg, Len);
	}
	else
	{
		Len = sprintf (Msg, "*** CONNECTED to %s\r", ConnectingCall);
		BPQSerialSendData(conn, Msg, Len);
		BPQSerialSetDCD(conn->hDevice);
	}
				
	channel->Connected = TRUE;

	return 0;

}

int KANTDisconnected (struct TNCDATA * conn, struct StreamInfo * channel, int Stream)
{
	UCHAR Msg[50];
	int Len;

	if (conn->MODE)
	{
		Len = sprintf (Msg, "S1%c*** DISCONNECTED", Stream + '@');
		SendKISSData(conn, Msg, Len);
	}
	else
	{
		BPQSerialSendData(conn, "*** DISCONNECTED\r", 17); 
		BPQSerialClrDCD(conn->hDevice);
	}

	channel->Connected = FALSE;
	channel->CloseTimer = 0;

	return 0;
}

// SCS Mode Stuff

unsigned short int compute_crc(unsigned char *buf,int len);
VOID EmCRCStuffAndSend(struct TNCDATA * conn, UCHAR * Msg, int Len);
int APIENTRY ChangeSessionPaclen(int Stream, int Paclen);

int EmUnstuff(UCHAR * MsgIn, UCHAR * MsgOut, int len)
{
	int i, j=0;

	for (i=0; i<len; i++, j++)
	{
		MsgOut[j] = MsgIn[i];
		if (MsgIn[i] == 170)			// Remove next byte
		{
			i++;
			if (MsgIn[i] != 0)
				if (i != len) return -1;
		}
	}

	return j;
}


UCHAR SCSReply[400];

int ReplyLen;


BOOL CheckStatusChange(struct TNCDATA * conn, int Channel, int BPQStream)
{
	int state, change;
	
	SessionState(BPQStream, &state, &change);

	if (change == 1)
	{
		if (state == 1)
		{
			// Connected

			GetCallsign(BPQStream, CONCALL);

			SCSReply[2] = Channel;
			SCSReply[3] = 3;
			ReplyLen  = sprintf(&SCSReply[4], "(%d) CONNECTED to %s", Channel, CONCALL);
			ReplyLen += 5;
			EmCRCStuffAndSend(conn, SCSReply, ReplyLen);

			return TRUE;
		}
		// Disconnected
		
		SCSReply[2] = Channel;
		SCSReply[3] = 3;
		ReplyLen  = sprintf(&SCSReply[4], "(%d) DISCONNECTED fm G8BPQ", Channel);
		ReplyLen += 5;		// Include Null
		EmCRCStuffAndSend(conn, SCSReply, ReplyLen);

		return TRUE;

	}

	return FALSE;

}

BOOL CheckForData(struct TNCDATA * conn,  struct StreamInfo * Channel, int HostStream, int BPQStream)
{
	int Length, Count;

	GetMsg(BPQStream, &SCSReply[5], &Length, &Count);

	if (Length == 0)
		return FALSE;

	if (strstr(&SCSReply[5], "} Downlink connect needs port number") ||
			strstr(&SCSReply[5], "} Failure with ") ||
			strstr(&SCSReply[5], "} Sorry, "))
		Channel->CloseTimer = CloseDelay * 10;
	else
		Channel->CloseTimer = 0;			// Cancel Timer

	SCSReply[2] = HostStream;
	SCSReply[3] = 7;
	SCSReply[4] = Length - 1;

	ReplyLen = Length + 5;
	EmCRCStuffAndSend(conn, SCSReply, ReplyLen);

	return TRUE;
}

VOID ProcessSCSHostFrame(struct TNCDATA * conn, UCHAR *  Buffer, int Length)
{
	int Channel = Buffer[0];
	int Command = Buffer[1] & 0x3f;
	int Len = Buffer[2];
	struct StreamInfo * channel;
	UCHAR TXBuff[400];
	int BPQStream;
	char * MYCall;
	UCHAR Stream;
	int TXLen, i;
	BPQVECSTRUC * HOST;

	// SCS Channel 31 is the Pactor channel, mapped to the first stream

	if (Channel == 0)
		Stream = -1;
	else
	if (Channel == 31)
		Stream = 0;
	else
		Stream = Channel;

	channel = conn->Channels[Stream];

	if (conn->Toggle == (Buffer[1] & 0x80) && (Buffer[1] & 0x40) == 0)
	{
		// Repeat Condition

		//EmCRCStuffAndSend(conn, SCSReply, ReplyLen);
		//return;
	}

	conn->Toggle = (Buffer[1] & 0x80);
	conn->Toggle ^= 0x80;

//	if (Channel == 255 &&  Len == 0)
	if (Channel == 255)
	{
		UCHAR * NextChan = &SCSReply[4];
		
		// General Poll

		// See if any channels have anything avaiilable

		for (i = 1; i <= conn->HOSTSTREAMS; i++)
		{
			channel = conn->Channels[i];
			HOST = &BPQHOSTVECTOR[channel->BPQStream - 1];		// API counts from 1
		
			if ((HOST->HOSTFLAGS & 3))
			{
				*(NextChan++) = i + 1;			// Something for this channel
				continue;
			}
		
			if (RXCount(channel->BPQStream))
				*(NextChan++) = i + 1;			// Something for this channel
		}

		*(NextChan++) = 0;

		SCSReply[2] = 255;
		SCSReply[3] = 1;

		ReplyLen = NextChan - &SCSReply[0];

		EmCRCStuffAndSend(conn, SCSReply, ReplyLen);
		return;
	}

	if (Channel == 254)			// Status
	{
		// Extended Status Poll

		SCSReply[2] = 254;
		SCSReply[3] = 7;		// Status
		SCSReply[4] = 3;		// Len -1
		SCSReply[5] = 0;
		SCSReply[6] = 0;
		SCSReply[7] = 0;
		SCSReply[8] = 0;

		ReplyLen = 9;
		EmCRCStuffAndSend(conn, SCSReply, 9);
		return;
	}


	if (Command == 0)
	{
		// Data Frame

		SendMsg(channel->BPQStream, &Buffer[3], Buffer[2]+ 1);

		goto AckIt;
	}

	switch (Buffer[3])
	{
	case 'J':				// JHOST

		conn->MODE = FALSE;
		DisableAppl(conn);

		return;

	case 'G':				// Specific Poll

		if (CheckStatusChange(conn, Channel, channel->BPQStream))
			return;						// It has sent reply

		if (CheckForData(conn, channel, Channel, channel->BPQStream))
			return;						// It has sent reply

		SCSReply[2] = Channel;
		SCSReply[3] = 0;
		ReplyLen = 4;
		EmCRCStuffAndSend(conn, SCSReply, 4);
		return;

	case 'C':				// Connect

		// Could be real, or just C to request status

		if (Channel == 0)
			goto AckIt;

		if (Length == 0)
		{
			// STATUS REQUEST - IF CONNECTED, GET CALL

			return;
		}
		Buffer[Length - 2] = 0;

		if (Buffer[5] = '%'	)			// Pacotr long path?
			TXLen = sprintf(TXBuff, "C %s\r", &Buffer[6]);
		else
			TXLen = sprintf(TXBuff, "C %s\r", &Buffer[5]);

		BPQStream = channel->BPQStream;
		MYCall = &channel->MYCall[0];

		if (MYCall[0] == 0)
			MYCall = (char *)&conn->MYCALL;

		Connect(BPQStream);
		if (MYCall[0] > 0)
		{
			ChangeSessionCallsign(BPQStream, EncodeCall(MYCall));
		}

		ChangeSessionPaclen(BPQStream, 100);

		SendMsg(BPQStream, TXBuff, TXLen);

	AckIt:

		SCSReply[2] = Channel;
		SCSReply[3] = 0;
		ReplyLen = 4;
		EmCRCStuffAndSend(conn, SCSReply, 4);
		return;

	case 'D':

		// Disconnect

		Disconnect(channel->BPQStream);
		goto AckIt;
		
	case '%':

		// %X commands

		switch (Buffer[4])
		{
		case 'V':					// Version

			SCSReply[2] = Channel;
			SCSReply[3] = 1;
			strcpy(&SCSReply[4], "4.8 1.32");
			ReplyLen = 13;
			EmCRCStuffAndSend(conn, SCSReply, 13);

			return;
		case 'M':
			
		default:
						
			SCSReply[2] = Channel;
			SCSReply[3] = 1;
			SCSReply[4] = 0;

			ReplyLen = 5;
			EmCRCStuffAndSend(conn, SCSReply, 5);

			return;
		}
	case '@':
	default:
						
		SCSReply[2] = Channel;
		SCSReply[3] = 1;
		SCSReply[4] = 0;

		ReplyLen = 5;
		EmCRCStuffAndSend(conn, SCSReply, 5);
	}
}


VOID ProcessSCSTextCommand(struct TNCDATA * conn, char * Command, int Len)
{
	// Command to SCS in non-Host mode.

	// We can probably just dump anything but JHOST 4 and MYCALL

	if (Len == 1)
		goto SendPrompt;		// Just a CR

	if (_memicmp(Command, "JHOST4", 6) == 0)
	{
		conn->MODE = TRUE;
		conn->Toggle = 0;
		EnableAppl(conn);

		return;
	}

	if (_memicmp(Command, "TERM 4", 6) == 0)
		conn->Term4Mode = TRUE;

	else if (_memicmp(Command, "T 0", 3) == 0)
		conn->Term4Mode = FALSE;

	else if (_memicmp(Command, "PAC 4", 5) == 0)
		conn->PACMode = TRUE;

	if (_memicmp(Command, "MYC", 3) == 0)
	{
		char * ptr = strchr(Command, ' ');
		char MYResp[80];

		Command[Len-1] = 0;		// Remove CR
		
		if (ptr && (strlen(ptr) > 2))
		{
			strcpy(conn->MYCALL, ++ptr); 
		}

		sprintf(MYResp, "\rMycall: >%s<", conn->MYCALL);
		PUTSTRING(conn, MYResp);
	}

	else if (_memicmp(Command, "SYS SERN", 8) == 0)
	{
		char SerialNo[] = "\r\nSerial number: 0100000000000000";
		PUTSTRING(conn, SerialNo);
	}
	else 
	{
		char SerialNo[] = "\rXXXX";
		PUTSTRING(conn, SerialNo);
	}

SendPrompt:	
	
	if (conn->PACMode)
	{
		PUTCHARx(conn, 13);
		PUTCHARx(conn, 'p');
		PUTCHARx(conn, 'a');
		PUTCHARx(conn, 'c');
		PUTCHARx(conn, ':');
		PUTCHARx(conn, ' ');

		return;
	}

	if (conn->Term4Mode)
	{
		PUTCHARx(conn, 13);
		PUTCHARx(conn, 4);
		PUTCHARx(conn, 13);
		PUTCHARx(conn, 'c');
		PUTCHARx(conn, 'm');
		PUTCHARx(conn, 'd');
		PUTCHARx(conn, ':');
		PUTCHARx(conn, ' ');
		PUTCHARx(conn, 1);
	}
	else
	{
		PUTCHARx(conn, 13);
		PUTCHARx(conn, 'c');
		PUTCHARx(conn, 'm');
		PUTCHARx(conn, 'd');
		PUTCHARx(conn, ':');
		PUTCHARx(conn, ' ');
	}


/*

	if (conn->Term4Mode)
		PUTCHARx(conn, 4);

	PUTCHARx(conn, 13);
	PUTCHARx(conn, 'c');
	PUTCHARx(conn, 'm');
	PUTCHARx(conn, 'd');
	PUTCHARx(conn, ':');
	PUTCHARx(conn, ' ');
	
*/
	return;
}


VOID ProcessSCSPacket(struct TNCDATA * conn, UCHAR * rxbuffer, int Length)
{
	unsigned short crc;
	char UnstuffBuffer[500];

	// DED mode doesn't have an end of frame delimiter. We need to know if we have a full frame

	// Fortunately this is a polled protocol, so we only get one frame at a time

	// If first char != 170, then probably a Terminal Mode Frame. Wait for CR on end

	// If first char is 170, we could check rhe length field, but that could be corrupt, as
	// we haen't checked CRC. All I can think of is to check the CRC and if it is ok, assume frame is
	// complete. If CRC is duff, we will eventually time out and get a retry. The retry code
	// can clear the RC buffer
			
Loop:

	if (rxbuffer[0] != 170)
	{
		UCHAR *ptr;
		int cmdlen;
		
		// Char Mode Frame I think we need to see CR on end (and we could have more than one in buffer

		// If we think we are in host mode, then to could be noise - just discard.

		if (conn->MODE)
		{
			conn->RXBPtr = 0;
			return;
		}

		rxbuffer[Length] = 0;

		if (rxbuffer[0] == 0x1b)
		{
			// Just ignore (I think!)

			conn->RXBPtr--;
			if (conn->RXBPtr)
			{
				memmove(rxbuffer, rxbuffer+1, conn->RXBPtr + 1);
				Length--;
				goto Loop;
			}
			return;
		}

		if (rxbuffer[0] == 0x1e)
		{
			// Status POLL

			conn->RXBPtr--;
			if (conn->RXBPtr)
			{
				memmove(rxbuffer, rxbuffer+1, conn->RXBPtr + 1);
				Length--;
				goto Loop;
			}
			PUTCHARx(conn, 30);
			PUTCHARx(conn, 0x87);
	if (conn->Term4Mode)
	{
		PUTCHARx(conn, 13);
		PUTCHARx(conn, 4);
		PUTCHARx(conn, 13);
		PUTCHARx(conn, 'c');
		PUTCHARx(conn, 'm');
		PUTCHARx(conn, 'd');
		PUTCHARx(conn, ':');
		PUTCHARx(conn, ' ');
		PUTCHARx(conn, 1);
	}
	else
	{
		PUTCHARx(conn, 13);
		PUTCHARx(conn, 'c');
		PUTCHARx(conn, 'm');
		PUTCHARx(conn, 'd');
		PUTCHARx(conn, ':');
		PUTCHARx(conn, ' ');
	}


			return;
		}
		ptr = strchr(rxbuffer, 13);

		if (ptr == 0)
			return;		// Wait for rest of frame

		ptr++;

		cmdlen = ptr - rxbuffer;

		// Complete Char Mode Frame

		conn->RXBPtr -= cmdlen;		// Ready for next frame
					
		ProcessSCSTextCommand(conn, rxbuffer, cmdlen);

		if (conn->RXBPtr)
		{
			memmove(rxbuffer, ptr, conn->RXBPtr + 1);
			if (conn->Mode)
			{
				// now in host mode, so pass rest up a level
				
				ProcessSCSPacket(conn, conn->RXBUFFER, conn->RXBPtr);
				return;
			}
			goto Loop;
		}
		return;
	}

	// Receiving a Host Mode frame

	if (Length < 6)				// Minimum Frame Sise
		return;

	if (rxbuffer[2] == 170)
	{
		// Retransmit Request
	
		conn->RXBPtr = 0;
		return;				// Ignore for now
	}

	// Can't unstuff into same buffer - fails if partial msg received, and we unstuff twice


	Length = EmUnstuff(&rxbuffer[2], &UnstuffBuffer[2], Length - 2);

	if (Length == -1)
	{
		// Unstuff returned an errors (170 not followed by 0)

		conn->RXBPtr = 0;
		return;				// Ignore for now
	}
	crc = compute_crc(&UnstuffBuffer[2], Length);

	if (crc == 0xf0b8)		// Good CRC
	{
		conn->RXBPtr = 0;		// Ready for next frame
		ProcessSCSHostFrame(conn, &UnstuffBuffer[2], Length);
		return;
	}

	// Bad CRC - assume incomplete frame, and wait for rest. If it was a full bad frame, timeout and retry will recover link.

	return;
}


VOID EmCRCStuffAndSend(struct TNCDATA * conn, UCHAR * Msg, int Len)
{
	unsigned short int crc;
	UCHAR StuffedMsg[500];
	int i, j;

	crc = compute_crc(&Msg[2], Len-2);
	crc ^= 0xffff;

	Msg[Len++] = (crc&0xff);
	Msg[Len++] = (crc>>8);

	for (i = j = 2; i < Len; i++)
	{
		StuffedMsg[j++] = Msg[i];
		if (Msg[i] == 170)
		{
			StuffedMsg[j++] = 0;
		}
	}

	if (j != i)
	{
		Len = j;
		memcpy(Msg, StuffedMsg, j);
	}

	Msg[0] = 170;
	Msg[1] = 170;

	BPQSerialSendData(conn, Msg, Len);
}


			