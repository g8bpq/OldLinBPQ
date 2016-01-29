#ifndef LINBPQ
#include "Rigresource.h"
#endif

#define IDI_ICON2 2

#define MAXBLOCK 4096

struct TimeScan
{
	time_t Start;
	time_t End;
	struct ScanEntry ** Scanlist;
};

struct ScanEntry
{
	//	Holds info for one frequency change. May Need to set Feeq, Mode, Repeater Split, Pactor/Winmor Bandwidth

	double Freq;		// In case needed to report to WL2K
	int Dwell;			// Dwell Time on this freq
	char Bandwidth;		// W/N For WINMOR
	char RPacketMode;	// Robust or Normal for Tracker
	char HFPacketMode;	// Robust or Normal for Tracker
	char PMaxLevel;		// Pactor Max Level
	char PMinLevel;		// Pactor Max Level
	char Antenna;
//	char Supress;		// Dont report this one to WL2K
	char * Cmd1;
	int Cmd1Len;
	char * Cmd2;
	int Cmd2Len;
	char * Cmd3;
	int Cmd3Len;
	char * PollCmd;
	int PollCmdLen;
	char APPL[13];		// Autoconnect APPL for this Freq
	char APPLCALL[10];	// Callsign for autoconnect application
};

struct RIGINFO
{
//	struct TRANSPORTENTRY * AttachedSession;

	int BPQtoRADIO_Q;			// Frames from switch for radio

	UINT BPQPort;				// Port this radio is attached to. Bit Map, as may be more than one port controlling radio
	int PortNum;				// Number of port that defined this rig
	
	int IC735;					// Old ICOM with shorter freq message

	struct _EXTPORTDATA * PortRecord[32]; // BPQ32 port record(s) for this rig (null terminated list)

	UCHAR RigAddr;
	int ScanStopped;			// Scanning enabled if zero. Bits used for interlocked scanning (eg winmor/pactor on same port
	int ScanCounter;
	int PollCounter;			// Don't poll too often;
	int ScanFreq;				// Scan Rate

	BOOL OKtoChange;			// Can Change
	BOOL WaitingForPermission;	//

	BOOL RIGOK;					// RIG is reponding

	int Session;				// BPQ L4 Record Number
	int	DebugDelay;	

	char RigName[10];

	struct TimeScan ** TimeBands;	// List of TimeBands/Frequencies
	int NumberofBands;

	char * FreqText;			// Frequency list in text format

	// Frequency list is a block of ScanEntry structs, each holding Set Freq/Mode commands in link format, null terminated

	struct ScanEntry ** FreqPtr;

	int PTTMode;				// PTT Control Flags.

	#define PTTRTS		1
	#define PTTDTR		2
	#define PTTCI_V		4

	int PTTTimer;				// PTT Timer watchdog

	#define PTTLimit 200

	struct RIGPORTINFO * PORT;		// For PTT Routines

	HWND hLabel;
	HWND hCAT;
	HWND hFREQ;
	HWND hMODE;
	HWND hSCAN;
	HWND hPTT;

	char * WEB_Label;
	char * WEB_CAT;
	char * WEB_FREQ;
	char * WEB_MODE;
	char WEB_SCAN;
	char WEB_PTT;

	double RigFreq;
	char Valchar[15];			// Freq as char string
	char CurrentBandWidth;

	char PTTOn[60];
	char PTTOff[60];
	int PTTOnLen;
	int PTTOffLen;

	char Poll[50];
	int PollLen;

	char PTTCATPort[4][10];
	HANDLE PTTCATHandles[4];

};

// PortType Equates

#define ICOM 1
#define YAESU 2
#define KENWOOD 3
#define PTT 4
#define ANT 5
#define FT100 6
#define FT2000 7
#define FLEX 8
#define M710 9
#define NMEA 10
#define FT1000 11
#define DUMMY 12
#define FT990 13

// Yease seem to have lots of variants of the same model

#define FT1000D	1
#define FT1000MP 2 


struct RIGPORTINFO
{
	int PortType;				// ICOM, Yaesu, Etc
	int YaesuVariant;			// Yaesu seems to have lots of incompatible subtypes
	char IOBASE[80];
	char PTTIOBASE[80];			// Port for Hardware PTT - may be same as control port.
	int SPEED;
	struct RIGINFO Rigs[10];	// Rigs off a port
	char * InitPtr;				// Next Command
	int CmdSent;				// Last Command sent
	int CmdStream;				// Stream last command was issued o
	int	ConfiguredRigs;			// Radios on this interface
	int CurrentRig;				// Radio last accessed.

	int Timeout;				// Timeout response counter
	int Retries;
	BOOL PORTOK;				// PORT is reponding

	HANDLE hDevice;					// COM device Handle
	int ReopenDelay;
	struct TNCINFO * PTC;			// Set to TNC record address if using an SCS PTC Radio Port
	HANDLE hPTTDevice;				// May use a different port for PTT
	UCHAR TXBuffer[500];			// Last message sent - saved for Retry
	int TXLen;						// Len of last sent
	UCHAR RXBuffer[500];			// Message being received - may not arrive all at once
	int RXLen;						// Data in RXBUffer
	BOOL AutoPoll;					// set if last command was a Timer poll 
	// Local ScanStruct for Interactive Commands
	struct ScanEntry * FreqPtr;		// Block we are currently sending.
	struct ScanEntry ScanEntry;	
	char Line2[10];
	char Line3[10];
};



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

#define W98_SERIAL_GETDATA     0x801
#define W98_SERIAL_SETDATA     0x802
