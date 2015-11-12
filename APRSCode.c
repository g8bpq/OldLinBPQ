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


// Module to implement APRS "New Paradigm" Digipeater and APRS-IS Gateway

// First Version, November 2011


#pragma data_seg("_BPQDATA")

#define _CRT_SECURE_NO_DEPRECATE 
#define _USE_32BIT_TIME_T	// Until the ASM code switches to 64 bit time

#include <stdio.h>
#include "CHeaders.h"
#include "bpq32.h"
#include <time.h>
#include "kernelresource.h"

#include "tncinfo.h"

#include "BPQAPRS.h"

#ifndef WIN32

#include <unistd.h>
#include <sys/mman.h>
#include <sys/un.h>

int sfd;
struct sockaddr_un my_addr, peer_addr;
socklen_t peer_addr_size;


#endif

#define MAXAGE 3600 * 12	  // 12 Hours
#define MAXCALLS 20			  // Max Flood, Trace and Digi
#define GATETIMELIMIT 40 * 60 // Don't gate to RF if station not heard for this time (40 mins)

static BOOL APIENTRY  GETSENDNETFRAMEADDR();
static VOID DoSecTimer();
static VOID DoMinTimer();
static APRSProcessLine(char * buf);
static BOOL APRSReadConfigFile();
VOID APRSISThread(BOOL Report);
unsigned long _beginthread( void( *start_address )(BOOL Report), unsigned stack_size, void * arglist);
VOID __cdecl Debugprintf(const char * format, ...);
VOID __cdecl Consoleprintf(const char * format, ...);
BOOL APIENTRY  Send_AX(PMESSAGE Block, DWORD Len, UCHAR Port);
VOID Send_AX_Datagram(PDIGIMESSAGE Block, DWORD Len, UCHAR Port);
char * strlop(char * buf, char delim);
int APRSDecodeFrame(char * msg, char * buffer, int Stamp, UINT Mask);		// Unsemaphored DecodeFrame
APRSSTATIONRECORD * UpdateHeard(UCHAR * Call, int Port);
BOOL CheckforDups(char * Call, char * Msg, int Len);
VOID ProcessQuery(char * Query);
VOID ProcessSpecificQuery(char * Query, int Port, char * Origin, char * DestPlusDigis);
VOID CheckandDigi(DIGIMESSAGE * Msg, int Port, int FirstUnused, int Digis, int Len);		
VOID SendBeacon(int toPort, char * Msg, BOOL SendISStatus, BOOL SendSOGCOG);
Dll BOOL APIENTRY PutAPRSMessage(char * Frame, int Len);
VOID ProcessAPRSISMsg(char * APRSMsg);
static VOID SendtoDigiPorts(PDIGIMESSAGE Block, DWORD Len, UCHAR Port);
APRSSTATIONRECORD * LookupStation(char * call);
BOOL OpenGPSPort();
void PollGPSIn();
int CountLocalStations();
BOOL SendAPPLAPRSMessage(char * Frame);
VOID SendAPRSMessage(char * Message, int toPort);
static VOID TCPConnect();
struct STATIONRECORD * DecodeAPRSISMsg(char * msg);
struct STATIONRECORD * ProcessRFFrame(char * buffer, int len);
VOID APRSSecTimer();
double Distance(double laa, double loa);
struct STATIONRECORD * FindStation(char * Call, BOOL AddIfNotFound);
VOID DecodeAPRSPayload(char * Payload, struct STATIONRECORD * Station);


BOOL ProcessConfig();

extern int SemHeldByAPI;
extern int APRSMONDECODE();
extern struct ConsoleInfo MonWindow;
extern char VersionString[];

// All data should be initialised to force into shared segment

static char ConfigClassName[]="CONFIG";

BPQVECSTRUC * APRSMONVECPTR;

extern int MONDECODE();
extern VOID * zalloc(int len);
extern BOOL StartMinimized;

extern char * PortConfig[];
extern char TextVerstring[];

extern HWND hConsWnd;
extern HKEY REGTREE;

static int SecTimer = 10;
static int MinTimer = 60;

BOOL APRSApplConnected = FALSE;  
BOOL APRSWeb = FALSE;  

UINT APPL_Q = 0;				// Queue of frames for APRS Appl
UINT APPLTX_Q = 0;				// Queue of frames from APRS Appl
UINT APRSPortMask = 0;

char APRSCall[10] = "";
char APRSDest[10] = "APBPQ1";

UCHAR AXCall[7] = "";

char CallPadded[10] = "         ";

int GPSPort = 0;
int GPSSpeed = 0;
char GPSRelay[80] = "";

BOOL GateLocal = FALSE;
double GateLocalDistance = 0.0;

int MaxDigisforIS = 7;			// Dont send to IS if more digis uued to reach us

char WXFileName[MAX_PATH];
char WXComment[80];
BOOL SendWX = FALSE;
int WXInterval = 30;
int WXCounter = 29 * 60;

char APRSCall[10];
char LoppedAPRSCall[10];

BOOL WXPort[32];				// Ports to send WX to

BOOL GPSOK = 0;

char LAT[] = "0000.00N";	// in standard APRS Format      
char LON[] = "00000.00W";	//in standard APRS Format

char HostName[80];			// for BlueNMEA
BOOL BlueNMEAOK = FALSE;
int BlueNMEATimer = 0;

double SOG, COG;		// From GPS

double Lat = 0.0;
double Lon = 0.0;

BOOL PosnSet = FALSE;
/*
The null position should be include the \. symbol (unknown/indeterminate
position). For example, a Position Report for a station with unknown position
will contain the coordinates …0000.00N\00000.00W.…
*/
char * FloodCalls = 0;			// Calls to relay using N-n without tracing
char * TraceCalls = 0;			// Calls to relay using N-n with tracing
char * DigiCalls = 0;			// Calls for normal relaying

UCHAR FloodAX[MAXCALLS][7] = {0};
UCHAR TraceAX[MAXCALLS][7] = {0};
UCHAR DigiAX[MAXCALLS][7] = {0};

int FloodLen[MAXCALLS];
int TraceLen[MAXCALLS];
int DigiLen[MAXCALLS];

int ISPort = 0;
char ISHost[256] = "";
int ISPasscode = 0;
char NodeFilter[1000] = "m/50";		// Filter when the isn't an application
char ISFilter[1000] = "m/50";		// Current Filter
char APPLFilter[1000] = "";			// Filter when an Applcation is running

extern BOOL IGateEnabled;

char StatusMsg[256] = "";			// Must be in shared segment
int StatusMsgLen = 0;

char * BeaconPath[33] = {0};

char CrossPortMap[33][33] = {0};
char APRSBridgeMap[33][33] = {0};

UCHAR BeaconHeader[33][10][7] = {""};	//	Dest, Source and up to 8 digis 
int BeaconHddrLen[33] = {0};			// Actual Length used

char CFGSYMBOL = 'a';
char CFGSYMSET = 'B';

char SYMBOL = '=';						// Unknown Locaton
char SYMSET = '/';

BOOL TraceDigi = FALSE;					// Add Trace to packets relayed on Digi Calls

int MaxTraceHops = 2;
int MaxFloodHops = 2;

int BeaconInterval = 0;
int MobileBeaconInterval = 0;
time_t LastMobileBeacon = 0;
int BeaconCounter = 0;
int IStatusCounter = 0;					// Used to send ?ISTATUS? Responses
int StatusCounter = 0;					// Used to send Status Messages

char RunProgram[128] = "";				// Program to start

BOOL APRSISOpen = FALSE;
int ISDelayTimer = 0;					// Time before trying to reopen APRS-IS link

char APRSDESTS[][7] = {"AIR*", "ALL*", "AP*", "BEACON", "CQ*", "GPS*", "DF*", "DGPS*", "DRILL*",
				"DX*", "ID*", "JAVA*", "MAIL*", "MICE*", "QST*", "QTH*", "RTCM*", "SKY*",
				"SPACE*", "SPC*", "SYM*", "TEL*", "TEST*", "TLM*", "WX*", "ZIP"};

UCHAR AXDESTS[30][7] = {""};
int AXDESTLEN[30] = {0};

UCHAR axTCPIP[7];
UCHAR axRFONLY[7];
UCHAR axNOGATE[7];

int MessageCount = 0;

struct PortInfo
{ 
	int Index;
	int ComPort;
	char PortType[2];
	BOOL NewVCOM;				// Using User Mode Virtual COM Driver
	int ReopenTimer;			// Retry if open failed delay
	int RTS;
	int CTS;
	int DCD;
	int DTR;
	int DSR;
	char Params[20];				// Init Params (eg 9600,n,8)
	char PortLabel[20];
	HANDLE hDevice;
	BOOL Created;
	BOOL PortEnabled;
	int FLOWCTRL;
	int gpsinptr;
#ifdef WIN32
	OVERLAPPED Overlapped;
	OVERLAPPED OverlappedRead;
#endif
	char GPSinMsg[160];
	int GPSTypeFlag;					// GPS Source flags
	BOOL RMCOnly;						// Only send RMC msgs to this port
};



struct PortInfo InPorts[1] = {0};

// Heard Station info

#define MAXHEARD 1000

int HEARDENTRIES = 0;
int MAXHEARDENTRIES = 0;
int MHLEN = sizeof(APRSSTATIONRECORD);

// Area is allocated as needed

APRSSTATIONRECORD MHTABLE[MAXHEARD] = {0};

APRSSTATIONRECORD * MHDATA = &MHTABLE[0];

static SOCKET sock = (SOCKET) NULL;

//Duplicate suppression Code

#define MAXDUPS 100			// Number to keep
#define DUPSECONDS 28		// Time to Keep

struct DUPINFO
{
	time_t DupTime;
	int DupLen;
	char  DupUser[8];		// Call in ax.35 format
	char  DupText[100];
};

struct DUPINFO DupInfo[MAXDUPS];

struct OBJECT
{
	struct OBJECT * Next;
	UCHAR Path[10][7];		//	Dest, Source and up to 8 digis 
	int PathLen;			// Actual Length used
	char Message[80];
	char PortMap[33];
	int	Interval;
	int Timer;
};

struct OBJECT * ObjectList;		// List of objects to send;

int ObjectCount = 0;

#include <math.h>

#define M_PI       3.14159265358979323846

int RetryCount = 4;
int RetryTimer = 45;
int ExpireTime = 120;
int TrackExpireTime = 1440;
BOOL SuppressNullPosn = FALSE;
BOOL DefaultNoTracks = FALSE;
BOOL LocalTime = TRUE;

int MaxStations = 500;

RECT Rect, MsgRect, StnRect;

char Key[80];

// function prototypes

VOID RefreshMessages();

// a few global variables

char APRSDir[MAX_PATH] = "BPQAPRS";
char DF[MAX_PATH];

#define	FEND	0xC0	// KISS CONTROL CODES 
#define	FESC	0xDB
#define	TFEND	0xDC
#define	TFESC	0xDD

int StationCount = 0;

UCHAR NextSeq = 1;

BOOL ImageChanged;
BOOL NeedRefresh = FALSE;
time_t LastRefresh = 0;

//	Stationrecords are stored in a shared memory segment. based at APRSStationMemory (normally 0x43000000)

//	A pointer to the first is placed at the start of this


struct STATIONRECORD ** StationRecords = NULL;
struct STATIONRECORD * StationRecordPool = NULL;

struct APRSMESSAGE * Messages = NULL;
struct APRSMESSAGE * OutstandingMsgs = NULL;

VOID SendObject(struct OBJECT * Object);
VOID MonitorAPRSIS(char * Msg, int MsgLen, BOOL TX);

#ifndef WIN32
#define WSAEWOULDBLOCK 11
#endif

HANDLE hMapFile;
UCHAR * APRSStationMemory = NULL;


int ISSend(SOCKET sock, char * Msg, int Len, int flags)
{
	int Loops = 0;
	int Sent;

	MonitorAPRSIS(Msg, Len, TRUE);

	Sent = send(sock, Msg, Len, flags);

	while (Sent != Len && Loops++ < 300)					// 10 secs max
	{					
		if ((Sent == SOCKET_ERROR) && (WSAGetLastError() != WSAEWOULDBLOCK))
			return SOCKET_ERROR;
		
		if (Sent > 0)					// something sent
		{
			Len -= Sent;
			memmove(Msg, &Msg[Sent], Len);
		}		
		
		Sleep(30);
		Sent = send(sock, Msg, Len, flags);
	}

	return Sent;
}

Dll BOOL APIENTRY Init_APRS()
{
	int i;
	char * DCall;

#ifndef LINBPQ
	HKEY hKey=0;
	int retCode, Vallen, Type; 
#else
#ifndef WIN32
	int fd;
	char RX_SOCK_PATH[] = "BPQAPRSrxsock";
	char TX_SOCK_PATH[] = "BPQAPRStxsock";
#endif
#endif
	struct STATIONRECORD * Stn1, * Stn2;

	// CLear tables in case a restart

	StationRecords = NULL;
	Messages = NULL;
	OutstandingMsgs = NULL;

	StationCount = 0;
	HEARDENTRIES = 0;
	MAXHEARDENTRIES = 0;

	memset(MHTABLE, 0, sizeof(MHTABLE));

	ConvToAX25(MYNODECALL, MYCALL);

	ConvToAX25("TCPIP", axTCPIP);
	ConvToAX25("RFONLY", axRFONLY);
	ConvToAX25("NOGATE", axNOGATE);

	memset(&FloodAX[0][0], 0, sizeof(FloodAX));
	memset(&TraceAX[0][0], 0, sizeof(TraceAX));
	memset(&DigiAX[0][0], 0, sizeof(DigiAX));

	APRSPortMask = 0;

	memset(BeaconPath, sizeof(BeaconPath), 0);

	memset(&CrossPortMap[0][0], 0, sizeof(CrossPortMap));
	memset(&APRSBridgeMap[0][0], 0, sizeof(APRSBridgeMap));

	for (i = 1; i <= NUMBEROFPORTS; i++)
	{
		CrossPortMap[i][i] = TRUE;			// Set Defaults - Same Port
		CrossPortMap[i][0] = TRUE;			// and APRS-IS
	}

	PosnSet = 0;
	ObjectList = NULL;
	ObjectCount = 0;

	ISPort = ISHost[0] = ISPasscode = 0;

	if (APRSReadConfigFile() == 0)
		return FALSE;

#ifdef LINBPQ

	// Create a Shared Memory Object

	APRSStationMemory = NULL;

#ifndef WIN32

	fd = shm_open("/BPQAPRSSharedMem", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1)
	{
		perror("Create Shared Memory");
		printf("Create APRS Shared Memory Failed\n");
	}
	else
	{
		if (ftruncate(fd, sizeof(struct STATIONRECORD) * (MaxStations + 1)) == -1)
		{
			perror("Extend Shared Memory");
			printf("Extend APRS Shared Memory Failed\n");
		}
		else
		{
			// Map shared memory object

			APRSStationMemory = mmap((void *)0x43000000, sizeof(struct STATIONRECORD) * (MaxStations + 1),
			     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);

			if (APRSStationMemory == MAP_FAILED)
			{
				perror("Map Shared Memory");
				printf("Map APRS Shared Memory Failed\n");
				APRSStationMemory = NULL;
			}
		}
	}

#endif

	if (APRSStationMemory == NULL)
	{
		printf("APRS not using shared memory\n");
		APRSStationMemory = malloc(sizeof(struct STATIONRECORD) * (MaxStations + 1));
	}

#else

	retCode = RegOpenKeyEx (REGTREE,
                "SOFTWARE\\G8BPQ\\BPQ32",    
                              0,
                              KEY_QUERY_VALUE,
                              &hKey);

	if (retCode == ERROR_SUCCESS)
	{
		Vallen = 4;
		retCode = RegQueryValueEx(hKey, "IGateEnabled", 0, &Type, (UCHAR *)&IGateEnabled, &Vallen);

/*
		// Restore GPS Position if GPS is configured and LAN/LON is not


		if (GPSPort && PosnSet == 0)
		{
			char LATLON[20];

			Vallen = 29;
			retCode = RegQueryValueEx(hKey, "GPS", 0, &Type, LATLON, &Vallen);

			if (retCode == 0)
			{
				memcpy(LAT, LATLON, 8);
				memcpy(LON, &LATLON[10], 9);

				PosnSet = TRUE;
			}
		}
*/
	}

	// Create Memory Mapping for Station List

   hMapFile = CreateFileMapping(
                 INVALID_HANDLE_VALUE,    // use paging file
                 NULL,                    // default security
                 PAGE_READWRITE,          // read/write access
                 0,                       // maximum object size (high-order DWORD)
                 sizeof(struct STATIONRECORD) * (MaxStations + 1), // maximum object size (low-order DWORD)
                 "BPQAPRSStationsMappingObject");                 // name of mapping object

   if (hMapFile == NULL)
   {
      Consoleprintf("Could not create file mapping object (%d).\n", GetLastError());
      return 0;
   }

   UnmapViewOfFile((void *)0x43000000);


   APRSStationMemory = (LPTSTR) MapViewOfFileEx(hMapFile,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        sizeof(struct STATIONRECORD) * (MaxStations + 1),
						(void *)0x43000000);

   if (APRSStationMemory == NULL)
   {
	   Consoleprintf("Could not map view of file (%d).\n", GetLastError());
	   CloseHandle(hMapFile);
	   return 0;
   }

#endif

   // First record has pointer to table

   memset(APRSStationMemory, 0, sizeof(struct STATIONRECORD) * (MaxStations + 1));

   Stn1  = (struct STATIONRECORD *)APRSStationMemory;

   StationRecords = (struct STATIONRECORD **)Stn1;
   
   Stn1++;
   
   StationRecordPool = Stn1;

   for (i = 1; i < MaxStations; i++)		// Already have first
   {	   
		Stn2 = Stn1;
		Stn2++;
		Stn1->Next = Stn2;

		Stn1 = Stn2;
   }

	if (PosnSet == 0)
	{
		SYMBOL = '.';
		SYMSET = '\\';				// Undefined Posn Symbol
	}
	else
	{
		// Convert posn to floating degrees

		char LatDeg[3], LonDeg[4];
		memcpy(LatDeg, LAT, 2);
		LatDeg[2]=0;
		Lat=atof(LatDeg) + (atof(LAT+2)/60);
	
		if (LAT[7] == 'S') Lat=-Lat;
		
		memcpy(LonDeg, LON, 3);
		LonDeg[3]=0;
		Lon=atof(LonDeg) + (atof(LON+3)/60);
       
		if (LON[8]== 'W') Lon=-Lon;

		SYMBOL = CFGSYMBOL;
		SYMSET = CFGSYMSET;
	}

	//	First record has control info for APRS Mapping App

	Stn1  = (struct STATIONRECORD *)APRSStationMemory;
	memcpy(Stn1->Callsign, APRSCall, 10);
	Stn1->Lat = Lat;
	Stn1->Lon = Lon;
	Stn1->LastPort = MaxStations;

#ifndef WIN32

	// Open unix socket for messaging app

	sfd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sfd == -1)
	{
		perror("Socket");
	}
	else
	{
		u_long param=1;
		ioctl(sfd, FIONBIO, &param);			// Set non-blocking

		memset(&my_addr, 0, sizeof(struct sockaddr_un));
		my_addr.sun_family = AF_UNIX;
		strncpy(my_addr.sun_path, TX_SOCK_PATH, sizeof(my_addr.sun_path) - 1);
	
		memset(&peer_addr, 0, sizeof(struct sockaddr_un));
		peer_addr.sun_family = AF_UNIX;
		strncpy(peer_addr.sun_path, RX_SOCK_PATH, sizeof(peer_addr.sun_path) - 1);

		unlink(TX_SOCK_PATH);

		if (bind(sfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_un)) == -1)
            perror("bind");
	}
#endif

	// Convert Dest ADDRS to AX.25

	for (i = 0; i < 26; i++)
	{
		DCall = &APRSDESTS[i][0];
		if (strchr(DCall, '*'))
			AXDESTLEN[i] = strlen(DCall) - 1;
		else
			AXDESTLEN[i] = 6;

		ConvToAX25(DCall, &AXDESTS[i][0]);
	}

	// Process any Object Definitions

	// Setup Heard Data Area

	HEARDENTRIES = 0;
	MAXHEARDENTRIES = MAXHEARD;

	APRSMONVECPTR->HOSTAPPLFLAGS = 0x80;		// Request Monitoring

	if (ISPort && IGateEnabled)
	{
		_beginthread(APRSISThread, 0, (VOID *) TRUE);
	}

	if (GPSPort)
		OpenGPSPort();

	WritetoConsole("APRS Digi/Gateway Enabled\n");

	APRSWeb = TRUE;

	// If a Run parameter was supplied, run the program

	if (RunProgram[0] == 0)
		return TRUE;

	#ifndef WIN32
	{
		char * arg_list[] = {NULL, NULL};
		pid_t child_pid;	

		signal(SIGCHLD, SIG_IGN); // Silently (and portably) reap children. 

		//	Fork and Exec ARDOP

		printf("Trying to start %s\n", RunProgram);

		arg_list[0] = RunProgram;
	 
    	/* Duplicate this process. */ 

		child_pid = fork (); 

		if (child_pid == -1) 
 		{    				
			printf ("APRS fork() Failed\n"); 
			return 0;
		}

		if (child_pid == 0) 
 		{    				
			execvp (arg_list[0], arg_list); 
        
			/* The execvp  function returns only if an error occurs.  */ 

			printf ("Failed to run %s\n", RunProgram); 
			exit(0);			// Kill the new process
		}
	}								 
#else
	{
		int n = 0;
		
		STARTUPINFO  SInfo;			// pointer to STARTUPINFO 
	    PROCESS_INFORMATION PInfo; 	// pointer to PROCESS_INFORMATION 

		SInfo.cb=sizeof(SInfo);
		SInfo.lpReserved=NULL; 
		SInfo.lpDesktop=NULL; 
		SInfo.lpTitle=NULL; 
		SInfo.dwFlags=0; 
		SInfo.cbReserved2=0; 
	  	SInfo.lpReserved2=NULL; 

		while (KillOldTNC(RunProgram) && n++ < 100)
		{
			Sleep(100);
		}

		if (!CreateProcess(RunProgram, NULL, NULL, NULL, FALSE,0 ,NULL ,NULL, &SInfo, &PInfo))
			Debugprintf("Failed to Start %s Error %d ", RunProgram, GetLastError());
	}
#endif

	return TRUE;
}

#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

BOOL APRSActive;

VOID APRSClose()
{
	APRSActive = FALSE;

	if (sock)
	{		
		shutdown(sock, SD_BOTH);
		Sleep(50);

		closesocket(sock);
	}
#ifdef WIN32
	if (InPorts[0].hDevice)
		CloseHandle(InPorts[0].hDevice);
#endif
}

Dll VOID APIENTRY Poll_APRS()
{
	char Msg[256];
	int numBytes;

	SecTimer--;

	if (SecTimer == 0)
	{
		SecTimer = 10;
		DoSecTimer();

		MinTimer--;

		if (MinTimer == 0)
		{
			MinTimer = 10;
			DoMinTimer();
		}
	}

#ifdef LINBPQ
#ifndef WIN32

	// Look for messages from App

	numBytes = recvfrom(sfd, Msg, 256, 0, NULL, NULL);

	if (numBytes > 0)
	{
		char To[10];
		struct STATIONRECORD * Station;

		memcpy(To, &Msg[1], 9);
		Station = FindStation(To, TRUE);

		if (Station)
		{
			Msg[numBytes] = 0;
			SendAPPLAPRSMessage(Msg);
		}
		else
			printf("Cant Send APRS Message - Station Table is full\n");
	}
#endif
#endif

	if (GPSPort)
		PollGPSIn();

	if (APPLTX_Q)
	{
		UINT * buffptr = Q_REM(&APPLTX_Q);
			
		if (buffptr[2] == -1)
			SendAPPLAPRSMessage((char *)&buffptr[3]);
		else
			SendAPRSMessage((char *)&buffptr[3], buffptr[2]);
			
		ReleaseBuffer(buffptr);
	}

	while (APRSMONVECPTR->HOSTTRACEQ)
	{
		int stamp, len;
		BOOL MonitorNODES = FALSE;
		UINT * monbuff;
		UCHAR * monchars;
		MESSAGE * Orig;
		int Digis = 0;
		MESSAGE * AdjBuff;		// Adjusted for digis
		BOOL FirstUnused = FALSE;
		int DigisUsed = 0;		// Digis used to reach us
		DIGIMESSAGE Msg = {0};
		int Port, i;
		char * DEST;
		unsigned char buffer[1024];
		char ISMsg[500];
		char * ptr1;
		char * Payload;
		char * ptr3;
		char * ptr4;
		BOOL ThirdParty = FALSE;
		BOOL NoGate = FALSE;
		APRSSTATIONRECORD * MH;
		char MsgCopy[500];
		int toPort;
		struct STATIONRECORD * Station;
	
#ifdef WIN32
		struct _EXCEPTION_POINTERS exinfo;
		char EXCEPTMSG[80] = "";
#endif		
		monbuff = Q_REM(&APRSMONVECPTR->HOSTTRACEQ);

		monchars = (UCHAR *)monbuff;
		AdjBuff = Orig = (MESSAGE *)monchars;	// Adjusted for digis

		Port = Orig->PORT;
		
		if (Port & 0x80)		// TX
		{
			ReleaseBuffer(monbuff);
			continue;
		}

//		if (CompareCalls(Orig->ORIGIN, AXCall))	// Our Packet
//		{
//			ReleaseBuffer(monbuff);
//			continue;
//		}

		if ((APRSPortMask & (1 << (Port - 1))) == 0)// Port in use for APRS?
		{
			ReleaseBuffer(monbuff);
			continue;
		}

		stamp = monbuff[88];

		if ((UCHAR)monchars[4] & 0x80)		// TX
		{
			ReleaseBuffer(monbuff);
			continue;
		}

		// See if digipeaters present. 

		while ((AdjBuff->ORIGIN[6] & 1) == 0 && Digis < 9)
		{
			UCHAR * temp = (UCHAR *)AdjBuff;
			temp += 7;
			AdjBuff = (MESSAGE *)temp;

			// If we have already digi'ed it, ignore (Dup Check my fail on slow links)

			if (AdjBuff->ORIGIN[6] & 0x80)
			{
				// Used Digi

				if (memcmp(AdjBuff->ORIGIN, AXCall, 6) == 0)
				{
					ReleaseBuffer(monbuff);
					return;
				}
				DigisUsed++;
			}
	
			if (memcmp(AdjBuff->ORIGIN, axTCPIP, 6) == 0)
				ThirdParty = TRUE;

			Digis ++;

			if (FirstUnused == FALSE && (AdjBuff->ORIGIN[6] & 0x80) == 0)
			{
				// Unused Digi - see if we should digi it

				FirstUnused = Digis;
		//		CheckDigi(buff, AdjBuff->ORIGIN);
			}

		}

		if (Digis > 8)
		{
			ReleaseBuffer(monbuff);
			continue;					// Corrupt
		}

		if (Digis)
		{
			if (memcmp(AdjBuff->ORIGIN, axNOGATE, 6) == 0 
				|| memcmp(AdjBuff->ORIGIN, axRFONLY, 6) == 0
				|| DigisUsed > MaxDigisforIS)

				// TOo many digis or Last digis is NOGATE or RFONLY - dont send to IS

				NoGate = TRUE;
		}
		if (AdjBuff->CTL != 3 || AdjBuff->PID != 0xf0)				// Only UI
		{
			ReleaseBuffer(monbuff);
			continue;			
		}

		// Bridge if requested

		for (toPort = 1; toPort <= NUMBEROFPORTS; toPort++)
		{
			if (APRSBridgeMap[Port][toPort])
			{
				MESSAGE * Buffer = GetBuff();
				struct PORTCONTROL * PORT;

				if (Buffer)
				{
					memcpy(Buffer, Orig, Orig->LENGTH);
					Buffer->PORT = toPort;
					PORT = GetPortTableEntryFromPortNum(toPort);
					if (PORT)
						PUT_ON_PORT_Q(PORT, Buffer);
					else
						ReleaseBuffer(Buffer);
				}	
			}
		}

		if (CheckforDups(Orig->ORIGIN, AdjBuff->L2DATA, Orig->LENGTH - Digis * 7 - 23))
		{	
			ReleaseBuffer(monbuff);
			continue;			
		}

		// Decode Frame to TNC2 Monitor Format

		len = APRSDecodeFrame((char *)monchars,  buffer, stamp, APRSPortMask);

		if(len == 0)
		{
			// Couldn't Decode

			ReleaseBuffer(monbuff);
			Debugprintf("APRS discarded frame - decode failed\n");
			continue;			
		}

		buffer[len] = 0;

		memcpy(MsgCopy, buffer, len);
		MsgCopy[len] = 0;

		// Do internal Decode

#ifdef WIN32

		strcpy(EXCEPTMSG, "ProcessRFFrame");

		__try 
		{

		Station = ProcessRFFrame(MsgCopy, len);
		}
		#include "StdExcept.c"

		}
#else
		Station = ProcessRFFrame(MsgCopy, len);
#endif

		memcpy(MsgCopy, buffer, len);			// Process RF Frame may have changed it
		MsgCopy[len] = 0;

		buffer[len++] = 10;
		buffer[len] = 0;
		ptr1 = &buffer[10];				// Skip Timestamp
		Payload = strchr(ptr1, ':') + 2; // Start of Payload
		ptr3 = strchr(ptr1, ' ');		// End of addresses
		*ptr3 = 0;

		// if digis, remove any unactioned ones

		if (Digis)
		{
			ptr4 = strchr(ptr1, '*');		// Last Used Digi

			if (ptr4)
			{
				// We need header up to ptr4

				*(ptr4) = 0;
			}
			else
			{
				// No digis actioned - remove them all

				ptr4 = strchr(ptr1, ',');		// End of Dest
				*ptr4 = 0;
			}
		}

		ptr4 = strchr(ptr1, '>');		// End of Source
		*ptr4++ = 0;

		MH = UpdateHeard(ptr1, Port);

		MH->Station = Station;

		if (ThirdParty)
		{
//			Debugprintf("Setting Igate Flag - %s", MsgCopy);
			MH->IGate = TRUE;			// if we've seen msgs to TCPIP, it must be an Igate
		}

		if (NoGate)
			goto NoIS;

		// I think all PID F0 UI frames go to APRS-IS,
		// Except General Queries, Frames Gated from IS to RF, and Messages Addressed to us

		// or should we process Query frames locally ??

		if (Payload[0] == '}')
			goto NoIS;

		if (Payload[0] == '?')
		{
			// General Query

			ProcessQuery(&Payload[1]);

			// ?? Should we pass addressed Queries to IS ??
	
			goto NoIS;
		}

		if (Payload[0] == ':' && memcmp(&Payload[1], CallPadded, 9) == 0)
		{
			// Message for us

			if (Payload[11] == '?')			// Only queries - the node doesnt do messaging
				ProcessSpecificQuery(&Payload[12], Port, ptr1, ptr4);

			goto NoIS;
		}

		if (APRSISOpen && CrossPortMap[Port][0])	// No point if not open
		{
			len = sprintf(ISMsg, "%s>%s,qAR,%s:%s", ptr1, ptr4, APRSCall, Payload);

			ISSend(sock, ISMsg, len, 0);
	
			ptr1 = strchr(ISMsg, 13);
			if (ptr1) *ptr1 = 0;
//			Debugprintf(">%s", ISMsg);
		}	
	
	NoIS:
	
		// See if it is an APRS frame

		// If MIC-E, we need to process, whatever the destination

		DEST = &Orig->DEST[0];

		for (i = 0; i < 26; i++)
		{
			if (memcmp(DEST, &AXDESTS[i][0], AXDESTLEN[i]) == 0)
				goto OK;
		}

		switch(AdjBuff->L2DATA[0])
		{
			case '`':
			case 0x27:					// '
			case 0x1c:
			case 0x1d:					// MIC-E

				break;
		//	default:

				// Not to an APRS Destination
			
//				ReleaseBuffer(monbuff);
//				continue;
		}

OK:
		// If there are unused digis, we may need to digi it.

		if (Digis == 0 || FirstUnused == 0)
		{
			// No Digis, so finished

			ReleaseBuffer(monbuff);
			continue;
		}

		// Copy frame to a DIGIMessage Struct

		memcpy(&Msg, monbuff, 21 + (7 * Digis));		// Header, Dest, Source, Addresses and Digis

		len = Msg.LENGTH - 21 - (7 * Digis);			// Payload Length (including CTL and PID

		memcpy(&Msg.CTL, &AdjBuff->CTL, len);

		// Pass to Digi Code

		CheckandDigi(&Msg, Port, FirstUnused, Digis, len);		// Digi if necessary		

		ReleaseBuffer(monbuff);
	}

	return;
}

VOID CheckandDigi(DIGIMESSAGE * Msg, int Port, int FirstUnused, int Digis, int Len)
{
	UCHAR * Digi = &Msg->DIGIS[--FirstUnused][0];
	UCHAR * Call;
	int Index = 0;
	int SSID;

	// Check ordinary digi first

	Call = &DigiAX[0][0];
	SSID = Digi[6] & 0x1e;

	while (*Call)
	{
		if ((memcmp(Digi, Call, 6) == 0) && ((Call[6] & 0x1e) == SSID))
		{
			// Trace Call if enabled

			if (TraceDigi)
				memcpy(Digi, AXCall, 7);
	
			// mark as used;
		
			Digi[6] |= 0x80;	// Used bit

			SendtoDigiPorts(Msg, Len, Port);
			return;
		}
		Call += 7;
		Index++;
	}

	Call = &TraceAX[0][0];
	Index = 0;

	while (*Call)
	{
		if (memcmp(Digi, Call, TraceLen[Index]) == 0)
		{
			// if possible move calls along
			// insert our call, set used
			// decrement ssid, and if zero, mark as used;

			SSID = (Digi[6] & 0x1E) >> 1;

			if (SSID == 0)	
				return;					// Shouldn't have SSID 0 for Rrace/Flood

			if (SSID > MaxTraceHops)
				SSID = MaxTraceHops;	// Enforce our limit

			SSID--;

			if (SSID ==0)				// Finihed with it ?
				Digi[6] = (SSID << 1) | 0xe0;	// Used and Fixed bits
			else
				Digi[6] = (SSID << 1) | 0x60;	// Fixed bits

			if (Digis < 8)
			{
				memmove(Digi + 7, Digi, (Digis - FirstUnused) * 7);
			}
				
			memcpy(Digi, AXCall, 7);
			Digi[6] |= 0x80;

			SendtoDigiPorts(Msg, Len, Port);

			return;
		}
		Call += 7;
		Index++;
	}

	Index = 0;
	Call = &FloodAX[0][0];

	while (*Call)
	{
		if (memcmp(Digi, Call, FloodLen[Index]) == 0)
		{
			// decrement ssid, and if zero, mark as used;

			SSID = (Digi[6] & 0x1E) >> 1;

			if (SSID == 0)	
				return;					// Shouldn't have SSID 0 for Trace/Flood

			if (SSID > MaxFloodHops)
				SSID = MaxFloodHops;	// Enforce our limit

			SSID--;

			if (SSID ==0)						// Finihed with it ?
				Digi[6] = (SSID << 1) | 0xe0;	// Used and Fixed bits
			else
				Digi[6] = (SSID << 1) | 0x60;	// Fixed bits

			SendtoDigiPorts(Msg, Len, Port);

			return;
		}
		Call += 7;
		Index++;
	}
}


  
static VOID SendtoDigiPorts(PDIGIMESSAGE Block, DWORD Len, UCHAR Port)
{
	//	Can't use API SENDRAW, as that tries to get the semaphore, which we already have
	//  Len is the Payload Length (from CTL onwards)
	// The message can contain DIGIS - The payload must be copied forwards if there are less than 8

	// We send to all ports enabled in CrossPortMap

	UCHAR * EndofDigis = &Block->CTL;
	int i = 0;
	int toPort;

	while (Block->DIGIS[i][0] && i < 8)
	{
		i++;
	}

	EndofDigis = &Block->DIGIS[i][0];
	*(EndofDigis -1) |= 1;					// Set End of Address Bit

	if (i != 8)
		memmove(EndofDigis, &Block->CTL, Len);

	Len = Len + (i * 7) + 14;					// Include Source, Dest and Digis

//	Block->DEST[6] &= 0x7e;						// Clear End of Call
//	Block->ORIGIN[6] |= 1;						// Set End of Call

//	Block->CTL = 3;		//UI

	for (toPort = 1; toPort <= NUMBEROFPORTS; toPort++)
	{
		if (CrossPortMap[Port][toPort])
			Send_AX((PMESSAGE)Block, Len, toPort);
	}
	return;

}

VOID Send_AX_Datagram(PDIGIMESSAGE Block, DWORD Len, UCHAR Port)
{
	//	Can't use API SENDRAW, as that tries to get the semaphore, which we already have

	//  Len is the Payload Length (CTL, PID, Data)

	// The message can contain DIGIS - The payload must be copied forwards if there are less than 8

	UCHAR * EndofDigis = &Block->CTL;

	int i = 0;

	while (Block->DIGIS[i][0] && i < 8)
	{
		i++;
	}

	EndofDigis = &Block->DIGIS[i][0];
	*(EndofDigis -1) |= 1;					// Set End of Address Bit

	if (i != 8)
		memmove(EndofDigis, &Block->CTL, Len); // Include PID

	Len = Len + (i * 7) + 14;					// Include Source, Dest and Digis

	Send_AX((PMESSAGE)Block, Len, Port);

	return;

}

static BOOL APRSReadConfigFile()
{
	char * Config;
	char * ptr1, * ptr2;

	char buf[256],errbuf[256];

	Config = PortConfig[34];		// Config fnom bpq32.cfg

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
	
			if (!APRSProcessLine(buf))
			{
				WritetoConsole("APRS Bad config record ");
				strcat(errbuf, "\r\n");
				WritetoConsole(errbuf);
			}
		}
		return TRUE;
	}
	return FALSE;
}

BOOL ConvertCalls(char * DigiCalls, UCHAR * AX, int * Lens)
{
	int Index = 0;
	char * ptr;
	char * Context;
	UCHAR Work[MAXCALLS][7] = {0};
	int Len[MAXCALLS] = {0};
		
	ptr = strtok_s(DigiCalls, ", ", &Context);

	while(ptr)
	{
		if (Index == MAXCALLS) return FALSE;

		ConvToAX25(ptr, &Work[Index][0]);
		Len[Index++] = strlen(ptr);
		ptr = strtok_s(NULL, ", ", &Context);
	}

	memcpy(AX, Work, sizeof(Work));
	memcpy(Lens, Len, sizeof(Len));
	return TRUE;
}



static APRSProcessLine(char * buf)
{
	char * ptr, * p_value;

	ptr = strtok(buf, "= \t\n\r");

	if(ptr == NULL) return (TRUE);

	if(*ptr =='#') return (TRUE);			// comment

	if(*ptr ==';') return (TRUE);			// comment


//	 OBJECT PATH=APRS,WIDE1-1 PORT=1,IS INTERVAL=30 TEXT=;444.80TRF*111111z4807.60N/09610.63Wr%156 R15m

	if (_stricmp(ptr, "OBJECT") == 0)
	{
		char * p_Path, * p_Port, * p_Text;
		int Interval;
		struct OBJECT * Object;
		int Digi = 2;
		char * Context;
		int SendTo;

		p_value = strtok(NULL, "=");
		if (p_value == NULL) return FALSE;
		if (_stricmp(p_value, "PATH"))
			return FALSE;

		p_Path = strtok(NULL, "\t\n\r ");
		if (p_Path == NULL) return FALSE;

		p_value = strtok(NULL, "=");
		if (p_value == NULL) return FALSE;
		if (_stricmp(p_value, "PORT"))
			return FALSE;

		p_Port = strtok(NULL, "\t\n\r ");
		if (p_Port == NULL) return FALSE;

		p_value = strtok(NULL, "=");
		if (p_value == NULL) return FALSE;
		if (_stricmp(p_value, "INTERVAL"))
			return FALSE;

		p_value = strtok(NULL, " \t");
		if (p_value == NULL) return FALSE;

		Interval = atoi(p_value);

		if (Interval == 0)
			return FALSE;

		p_value = strtok(NULL, "=");
		if (p_value == NULL) return FALSE;
		if (_stricmp(p_value, "TEXT"))
			return FALSE;

		p_Text = strtok(NULL, "\n\r");
		if (p_Text == NULL) return FALSE;

		Object = zalloc(sizeof(struct OBJECT));

		if (Object == NULL)
			return FALSE;

		Object->Next = ObjectList;
		ObjectList = Object;

		if (Interval < 10)
			Interval = 10;

		Object->Interval = Interval;
		Object->Timer = (ObjectCount++) * 10 + 30;	// Spread them out;

		// Convert Path to AX.25 

		ConvToAX25(APRSCall, &Object->Path[1][0]);

		ptr = strtok_s(p_Path, ",\t\n\r", &Context);

		if (_stricmp(ptr, "APRS") == 0)			// First is Dest
			ConvToAX25(APRSDest, &Object->Path[0][0]);
		else if (_stricmp(ptr, "APRS-0") == 0)
			ConvToAX25("APRS", &Object->Path[0][0]);
		else
			ConvToAX25(ptr, &Object->Path[0][0]);
		
		ptr = strtok_s(NULL, ",\t\n\r", &Context);

		while (ptr)
		{
			ConvToAX25(ptr, &Object->Path[Digi++][0]);
			ptr = strtok_s(NULL, " ,\t\n\r", &Context);
		}

		Object->PathLen = Digi * 7;

		// Process Port List

		ptr = strtok_s(p_Port, ",", &Context);

		while (ptr)
		{
			SendTo = atoi(ptr);				// this gives zero for IS
	
			if (SendTo > NUMBEROFPORTS)
				return FALSE;

			Object->PortMap[SendTo] = TRUE;	
			ptr = strtok_s(NULL, " ,\t\n\r", &Context);
		}

		strcpy(Object->Message, p_Text);
		return TRUE;
	}

	if (_stricmp(ptr, "STATUSMSG") == 0)
	{
		p_value = strtok(NULL, ";\t\n\r");
		memcpy(StatusMsg, p_value, 255);	// Just in case too long
		StatusMsgLen = strlen(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "WXFileName") == 0)
	{
		p_value = strtok(NULL, ";\t\n\r");
		strcpy(WXFileName, p_value);
		SendWX = TRUE;
		return TRUE;
	}
	if (_stricmp(ptr, "WXComment") == 0)
	{
		p_value = strtok(NULL, ";\t\n\r");

		if (strlen(p_value) > 79)
			p_value[80] = 0;

		strcpy(WXComment, p_value);
		return TRUE;
	}


	if (_stricmp(ptr, "ISFILTER") == 0)
	{
		p_value = strtok(NULL, ";\t\n\r");
		strcpy(ISFilter, p_value);
		strcpy(NodeFilter, ISFilter);
		return TRUE;
	}

	if (_stricmp(ptr, "ReplaceDigiCalls") == 0)
	{
		TraceDigi = TRUE;
		return TRUE;
	}

	p_value = strtok(NULL, " \t\n\r");

	if (p_value == NULL)
		return FALSE;

	if (_stricmp(ptr, "APRSCALL") == 0)
	{
		strcpy(APRSCall, p_value);
		strcpy(LoppedAPRSCall, p_value);
		memcpy(CallPadded, APRSCall, strlen(APRSCall));	// Call Padded to 9 chars for APRS Messaging

		// Convert to ax.25

		return ConvToAX25(APRSCall, AXCall);
	}

	if (_stricmp(ptr, "APRSPATH") == 0)
	{
		int Digi = 2;
		int Port;
		char * Context;

		p_value = strtok_s(p_value, "=\t\n\r", &Context);

		Port = atoi(p_value);

		if (GetPortTableEntryFromPortNum(Port) == NULL)
			return FALSE;

		APRSPortMask |= 1 << (Port - 1);

		if (Context == NULL || Context[0] == 0)
			return TRUE;					// No dest - a receive-only port

		BeaconPath[Port] = _strdup(_strupr(Context));
	
		ptr = strtok_s(NULL, ",\t\n\r", &Context);

		if (ptr == NULL)
			return FALSE;

		ConvToAX25(APRSCall, &BeaconHeader[Port][1][0]);

		if (_stricmp(ptr, "APRS") == 0)			// First is Dest
			ConvToAX25(APRSDest, &BeaconHeader[Port][0][0]);
		else if (_stricmp(ptr, "APRS-0") == 0)
			ConvToAX25("APRS", &BeaconHeader[Port][0][0]);
		else
			ConvToAX25(ptr, &BeaconHeader[Port][0][0]);
		
		ptr = strtok_s(NULL, ",\t\n\r", &Context);

		while (ptr)
		{
			ConvToAX25(ptr, &BeaconHeader[Port][Digi++][0]);
			ptr = strtok_s(NULL, " ,\t\n\r", &Context);
		}

		BeaconHddrLen[Port] = Digi * 7;

		return TRUE;
	}

	if (_stricmp(ptr, "DIGIMAP") == 0)
	{
		int DigiTo;
		int Port;
		char * Context;

		p_value = strtok_s(p_value, "=\t\n\r", &Context);

		Port = atoi(p_value);

		if (GetPortTableEntryFromPortNum(Port) == NULL)
			return FALSE;

		CrossPortMap[Port][Port] = FALSE;	// Cancel Default mapping
		CrossPortMap[Port][0] = FALSE;		// Cancel Default APRSIS

		if (Context == NULL || Context[0] == 0)
			return FALSE;

		ptr = strtok_s(NULL, ",\t\n\r", &Context);

		while (ptr)
		{
			DigiTo = atoi(ptr);				// this gives zero for IS
	
			if (DigiTo > NUMBEROFPORTS)
				return FALSE;

			CrossPortMap[Port][DigiTo] = TRUE;	
			ptr = strtok_s(NULL, " ,\t\n\r", &Context);
		}

		return TRUE;
	}
	if (_stricmp(ptr, "BRIDGE") == 0)
	{
		int DigiTo;
		int Port;
		char * Context;

		p_value = strtok_s(p_value, "=\t\n\r", &Context);

		Port = atoi(p_value);

		if (GetPortTableEntryFromPortNum(Port) == NULL)
			return FALSE;

		if (Context == NULL)
			return FALSE;
	
		ptr = strtok_s(NULL, ",\t\n\r", &Context);

		while (ptr)
		{
			DigiTo = atoi(ptr);				// this gives zero for IS
	
			if (DigiTo > NUMBEROFPORTS)
				return FALSE;

			APRSBridgeMap[Port][DigiTo] = TRUE;	
			ptr = strtok_s(NULL, " ,\t\n\r", &Context);
		}

		return TRUE;
	}


	if (_stricmp(ptr, "BeaconInterval") == 0)
	{
		BeaconInterval = atoi(p_value);

		if (BeaconInterval < 5)
			BeaconInterval = 5;

		if (BeaconInterval)
			BeaconCounter = 30;				// Send first after 30 secs

		return TRUE;
	}

	if (_stricmp(ptr, "MobileBeaconInterval") == 0)
	{
		MobileBeaconInterval = atoi(p_value) * 60;
		return TRUE;
	}
	if (_stricmp(ptr, "MobileBeaconIntervalSecs") == 0)
	{
		MobileBeaconInterval = atoi(p_value);
		if (MobileBeaconInterval < 10)
			MobileBeaconInterval = 10;

		return TRUE;
	}

	if (_stricmp(ptr, "TRACECALLS") == 0)
	{
		TraceCalls = _strdup(_strupr(p_value));
		ConvertCalls(TraceCalls, &TraceAX[0][0], &TraceLen[0]);
		return TRUE;
	}

	if (_stricmp(ptr, "FLOODCALLS") == 0)
	{
		FloodCalls = _strdup(_strupr(p_value));
		ConvertCalls(FloodCalls, &FloodAX[0][0], &FloodLen[0]);
		return TRUE;
	}

	if (_stricmp(ptr, "DIGICALLS") == 0)
	{
		char AllCalls[1024];
		
		DigiCalls = _strdup(_strupr(p_value));
		strcpy(AllCalls, APRSCall);
		strcat(AllCalls, ",");
		strcat(AllCalls, DigiCalls);
		ConvertCalls(AllCalls, &DigiAX[0][0], &DigiLen[0]);
		return TRUE;
	}

	if (_stricmp(ptr, "MaxStations") == 0)
	{
		MaxStations = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "MaxAge") == 0)
	{
		ExpireTime = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "GPSPort") == 0)
	{
		GPSPort = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "GPSSpeed") == 0)
	{
		GPSSpeed = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "GPSRelay") == 0)
	{
		if (strlen(p_value) > 79)
			return FALSE;

		strcpy(GPSRelay, p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "BlueNMEA") == 0)
	{
		if (strlen(p_value) > 70)
			return FALSE;

		strcpy(HostName, p_value);
		return TRUE;
	}
	if (_stricmp(ptr, "LAT") == 0)
	{
		if (strlen(p_value) != 8)
			return FALSE;

		memcpy(LAT, _strupr(p_value), 8);
		PosnSet = TRUE;
		return TRUE;
	}

	if (_stricmp(ptr, "LON") == 0)
	{
		if (strlen(p_value) != 9)
			return FALSE;

		memcpy(LON, _strupr(p_value), 9);
		PosnSet = TRUE;
		return TRUE;
	}

	if (_stricmp(ptr, "SYMBOL") == 0)
	{
		if (p_value[0] > ' ' && p_value[0] < 0x7f)
			CFGSYMBOL = p_value[0];

		return TRUE;
	}

	if (_stricmp(ptr, "SYMSET") == 0)
	{
		CFGSYMSET = p_value[0];
		return TRUE;
	}

	if (_stricmp(ptr, "MaxTraceHops") == 0)
	{
		MaxTraceHops = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "MaxFloodHops") == 0)
	{
		MaxFloodHops = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "ISHOST") == 0)
	{
		strncpy(ISHost, p_value, 250);
		return TRUE;
	}

	if (_stricmp(ptr, "ISPORT") == 0)
	{
		ISPort = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "ISPASSCODE") == 0)
	{
		ISPasscode = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "MaxDigisforIS") == 0)
	{
		MaxDigisforIS = atoi(p_value);
		return TRUE;
	}

	if (_stricmp(ptr, "GateLocalDistance") == 0)
	{
		GateLocalDistance = atoi(p_value);
		if (GateLocalDistance > 0.0)
			GateLocal = TRUE;

		return TRUE;
	}

	if (_stricmp(ptr, "WXInterval") == 0)
	{
		WXInterval = atoi(p_value);
		WXCounter = (WXInterval - 1) * 60;
		return TRUE;
	}

	if (_stricmp(ptr, "WXPortList") == 0)
	{
		char ParamCopy[80];
		char * Context;
		int Port;
		char * ptr;
		int index = 0;

		for (index = 0; index < 32; index++)
			WXPort[index] = FALSE;
	
		if (strlen(p_value) > 79)
			p_value[80] = 0;

		strcpy(ParamCopy, p_value);
		
		ptr = strtok_s(ParamCopy, " ,\t\n\r", &Context);

		while (ptr)
		{
			Port = atoi(ptr);				// this gives zero for IS
	
			WXPort[Port] = TRUE;	
			
			ptr = strtok_s(NULL, " ,\t\n\r", &Context);
		}
		return TRUE;
	}

	if (_stricmp(ptr, "Run") == 0)
	{
		strcpy(RunProgram, p_value);
		return TRUE;
	}


	//
	//	Bad line
	//
	return (FALSE);	
}

VOID SendAPRSMessage(char * Message, int toPort)
{
	int Port;
	DIGIMESSAGE Msg;
	
	int Len;

	// toPort = -1 means all tadio ports. 0 = IS

	if (toPort == -1)
	{
		for (Port = 1; Port <= NUMBEROFPORTS; Port++)
		{
			if (BeaconHddrLen[Port])		// Only send to ports with a DEST defined
			{
				memcpy(Msg.DEST, &BeaconHeader[Port][0][0],  10 * 7);
				Msg.PID = 0xf0;
				Msg.CTL = 3;
				Len = sprintf(Msg.L2DATA, "%s", Message);
				Send_AX_Datagram(&Msg, Len + 2, Port);
			}
		}

		return;
	}

	if (toPort == 0 && APRSISOpen)
	{
		char ISMsg[300];

		Len = sprintf(ISMsg, "%s>%s,TCPIP*:%s\r\n", APRSCall, APRSDest, Message);
		ISSend(sock, ISMsg, Len, 0);
	}

	if (toPort && BeaconHddrLen[toPort])
	{
		memcpy(Msg.DEST, &BeaconHeader[toPort][0][0], 10 * 7);
		Msg.PID = 0xf0;
		Msg.CTL = 3;
		Len = sprintf(Msg.L2DATA, "%s", Message);
		Send_AX_Datagram(&Msg, Len + 2, toPort);

		return;
	}
}


VOID ProcessSpecificQuery(char * Query, int Port, char * Origin, char * DestPlusDigis)
{
	if (_memicmp(Query, "APRSS", 5) == 0)
	{
		char Message[255];
	
		sprintf(Message, ":%-9s:%s", Origin, StatusMsg);
		SendAPRSMessage(Message, Port);

		return;
	}

	if (_memicmp(Query, "APRST", 5) == 0 || _memicmp(Query, "PING?", 5) == 0)
	{
		// Trace Route
		//:KH2ZV   :?APRST :N8UR     :KH2Z>APRS,DIGI1,WIDE*:
		//:G8BPQ-14 :Path - G8BPQ-14>APU25N

		char Message[255];
	
		sprintf(Message, ":%-9s:Path - %s>%s", Origin, Origin, DestPlusDigis);
		SendAPRSMessage(Message, Port);

		return;
	}
}

VOID ProcessQuery(char * Query)
{
	if (memcmp(Query, "IGATE?", 6) == 0)
	{
		IStatusCounter = (rand() & 31) + 5;			// 5 - 36 secs delay
		return;
	}

	if (memcmp(Query, "APRS?", 5) == 0)
	{
		BeaconCounter = (rand() & 31) + 5;			// 5 - 36 secs delay
		return;
	}
}
Dll VOID APIENTRY APISendBeacon()
{
	BeaconCounter = 2;
}

VOID SendBeacon(int toPort, char * BeaconText, BOOL SendISStatus, BOOL SendSOGCOG)
{
	int Port;
	DIGIMESSAGE Msg;
	char * StMsg = BeaconText;
	int Len;
	char SOGCOG[10] = "";
	struct STATIONRECORD * Station;
	
	if (PosnSet == FALSE)
		return;

	if (SendSOGCOG | (COG != 0.0))
		sprintf(SOGCOG, "%03.0f/%03.0f", COG, SOG);

	BeaconCounter = BeaconInterval * 60;

	if (StMsg == NULL)
		StMsg = StatusMsg;
	
	if (ISPort && IGateEnabled)
		Len = sprintf(Msg.L2DATA, "%c%s%c%s%c%s BPQ32 Igate V %s", (APRSApplConnected) ? '=' : '!',
			LAT, SYMSET, LON, SYMBOL, SOGCOG, VersionString);
	else
		Len = sprintf(Msg.L2DATA, "%c%s%c%s%c%s BPQ32 V %s", (APRSApplConnected) ? '=' : '!',
			LAT, SYMSET, LON, SYMBOL, SOGCOG, VersionString);
	
	Msg.PID = 0xf0;
	Msg.CTL = 3;

	// Add to dup check list, so we wont digi it if we here it back 
	// Should we drop it if we've sent it recently ??

	if (CheckforDups(APRSCall, Msg.L2DATA, Len - 23))
		return;

	// Add to our station list

	Station = FindStation(APRSCall, TRUE);
		
	strcpy(Station->Path, "APBPQ1");
	strcpy(Station->LastPacket, Msg.L2DATA);
//	Station->LastPort = Port;

	DecodeAPRSPayload(Msg.L2DATA, Station);
	Station->TimeLastUpdated = time(NULL);



	if (toPort && BeaconHddrLen[toPort])
	{
		memcpy(Msg.DEST, &BeaconHeader[toPort][0][0], 10 * 7);		// Clear unused digis
		Send_AX_Datagram(&Msg, Len + 2, toPort);

		return;
	}

	for (Port = 1; Port <= NUMBEROFPORTS; Port++)
	{
		if (BeaconHddrLen[Port])		// Only send to ports with a DEST defined
		{
	if (ISPort && IGateEnabled)
		Len = sprintf(Msg.L2DATA, "%c%s%c%s%c%s BPQ32 Igate V %s", (APRSApplConnected) ? '=' : '!',
			LAT, SYMSET, LON, SYMBOL, SOGCOG, VersionString);
	else
		Len = sprintf(Msg.L2DATA, "%c%s%c%s%c%s BPQ32 V %s", (APRSApplConnected) ? '=' : '!',
			LAT, SYMSET, LON, SYMBOL, SOGCOG, VersionString);
			Msg.PID = 0xf0;
			Msg.CTL = 3;

			memcpy(Msg.DEST, &BeaconHeader[Port][0][0], 10 * 7);
			Send_AX_Datagram(&Msg, Len + 2, Port);
		}
	}

	// Also send to APRS-IS if connected

	if (APRSISOpen)
	{
		char ISMsg[300];

		Len = sprintf(ISMsg, "%s>%s,TCPIP*:%c%s%c%s%c%s BPQ32 Igate V %s\r\n", APRSCall, APRSDest,
			(APRSApplConnected) ? '=' : '!', LAT, SYMSET, LON, SYMBOL, SOGCOG, VersionString);

		ISSend(sock, ISMsg, Len, 0);
		Debugprintf(">%s", ISMsg);

		if (SendISStatus)
			IStatusCounter = 5;
	}

	if (SendISStatus)
		StatusCounter = 10;

}

VOID SendObject(struct OBJECT * Object)
{
	int Port;
	DIGIMESSAGE Msg;
	int Len;
	
	//	Add to dup list in case we get it back

	CheckforDups(APRSCall, Object->Message, strlen(Object->Message));

	for (Port = 1; Port <= NUMBEROFPORTS; Port++)
	{
		if (Object->PortMap[Port])
		{
			Msg.PID = 0xf0;
			Msg.CTL = 3;
			Len = sprintf(Msg.L2DATA, "%s", Object->Message);
			memcpy(Msg.DEST, &Object->Path[0][0],  Object->PathLen + 1);
			Send_AX_Datagram(&Msg, Len + 2, Port);
		}
	}

	// Also send to APRS-IS if connected

	if (APRSISOpen && Object->PortMap[0])
	{
		char ISMsg[300];
		Len = sprintf(ISMsg, "%s>%s,TCPIP*:%s\r\n", APRSCall, APRSDest, Object->Message);
		ISSend(sock, ISMsg, Len, 0);

	}
}



VOID SendStatus(char * StatusText)
{
	int Port;
	DIGIMESSAGE Msg;
	int Len;

	if (APRSISOpen)
	{
		Msg.PID = 0xf0;
		Msg.CTL = 3;

		Len = sprintf(Msg.L2DATA, ">%s", StatusText);

		for (Port = 1; Port <= NUMBEROFPORTS; Port++)
		{
			if (BeaconHddrLen[Port])		// Only send to ports with a DEST defined
			{
				memcpy(Msg.DEST, &BeaconHeader[Port][0][0], 10 * 7);
				Send_AX_Datagram(&Msg, Len + 2, Port);
			}
		}

		Len = sprintf(Msg.L2DATA, "%s>%s,TCPIP*:>%s\r\n", APRSCall, APRSDest, StatusText);
		ISSend(sock, Msg.L2DATA, Len, 0);
//		Debugprintf(">%s", Msg.L2DATA);
	}
}


VOID SendIStatus()
{
	int Port;
	DIGIMESSAGE Msg;
	int Len;

	if (APRSISOpen)
	{
		Msg.PID = 0xf0;
		Msg.CTL = 3;

		Len = sprintf(Msg.L2DATA, "<IGATE,MSG_CNT=%d,LOC_CNT=%d", MessageCount , CountLocalStations());

		for (Port = 1; Port <= NUMBEROFPORTS; Port++)
		{
			if (BeaconHddrLen[Port])		// Only send to ports with a DEST defined
			{
				memcpy(Msg.DEST, &BeaconHeader[Port][0][0], 10 * 7);
				Send_AX_Datagram(&Msg, Len + 2, Port);
			}
		}

		Len = sprintf(Msg.L2DATA, "%s>%s,TCPIP*:<IGATE,MSG_CNT=%d,LOC_CNT=%d\r\n", APRSCall, APRSDest, MessageCount, CountLocalStations());
		ISSend(sock, Msg.L2DATA, Len, 0);
//		Debugprintf(">%s", Msg.L2DATA);
	}
}


VOID DoSecTimer()
{
	struct OBJECT * Object = ObjectList;

	while (Object)
	{
		Object->Timer--;

		if (Object->Timer == 0)
		{
			Object->Timer = 60 * Object->Interval;
			SendObject(Object);
		}
		Object = Object->Next;
	}

	if (ISPort && APRSISOpen == 0 && IGateEnabled)
	{
		ISDelayTimer++;

		if (ISDelayTimer > 60)
		{
			ISDelayTimer = 0;
			_beginthread(APRSISThread, 0, (VOID *) TRUE);
		}
	}

	if (HostName[0])
	{
		if (BlueNMEAOK == 0)
		{
			BlueNMEATimer++;
			if (BlueNMEATimer > 15)
			{
				BlueNMEATimer = 0;
				_beginthread(TCPConnect,0,0);
			}
		}
	}


	if (BeaconCounter)
	{
		BeaconCounter--;

		if (BeaconCounter == 0)
		{
			BeaconCounter = BeaconInterval * 60;
			SendBeacon(0, StatusMsg, TRUE, FALSE);
		}
	}

	if (StatusCounter)
	{
		StatusCounter--;

		if (StatusCounter == 0)
		{
			SendStatus(StatusMsg);
		}
	}
	if (IStatusCounter)
	{
		IStatusCounter--;

		if (IStatusCounter == 0)
		{
			SendIStatus();
		}
	}

	if (GPSOK)
	{
		GPSOK--;

		if (GPSOK == 0)
#ifdef LINBPQ
			Debugprintf("GPS Lost");
#else
			SetDlgItemText(hConsWnd, IDC_GPS, "No GPS");
#endif
	}

	APRSSecTimer();				// Code from APRS APPL
}

int CountPool()
{
	struct STATIONRECORD * ptr = StationRecordPool;
	int n = 0;

	while (ptr)
	{
		n++;
		ptr = ptr->Next;
	}
	return n;
}

static VOID DoMinTimer()
{
	struct STATIONRECORD * ptr = *StationRecords;
	struct STATIONRECORD * last = NULL;
	time_t AgeLimit = time(NULL ) - (ExpireTime * 60);
	int i = 0;

	// Remove old records

	while (ptr)
	{
		if (ptr->TimeLastUpdated < AgeLimit)
		{
			StationCount--;

			if (last)
			{
				last->Next = ptr->Next;
			
				// Put on front of free chain

				ptr->Next = StationRecordPool;
				StationRecordPool = ptr;

				ptr = last->Next;
			}
			else
			{
				// First in list
				
				*StationRecords = ptr->Next;
			
				// Put on front of free chain

				ptr->Next = StationRecordPool;
				StationRecordPool = ptr;

				if (*StationRecords)
				{
					ptr = *StationRecords;
				}
				else
				{
					ptr = NULL;
					CountPool();
				}
			}
		}
		else
		{
			last = ptr;
			ptr = ptr->Next;
		}
	}
}

char APRSMsg[300];

int ISHostIndex = 0;

VOID APRSISThread(BOOL Report)
{
	// Receive from core server

	char Signon[500];
	unsigned char work[4];

	struct sockaddr_in sinx; 
	int addrlen=sizeof(sinx);
	struct addrinfo hints, *res = 0, *saveres;
	int len, err;
	u_long param=1;
	BOOL bcopt=TRUE;
	char Buffer[1000];
	int InputLen = 1;		// Non-zero
	char errmsg[100];
	char * ptr;
	int inptr = 0;
	char APRSinMsg[1000];
	char PortString[20];
	char host[256];
	char serv[256];

	Debugprintf("BPQ32 APRS IS Thread");
#ifndef LINBPQ
	SetDlgItemText(hConsWnd, IGATESTATE, "IGate State: Connecting");
#endif

	if (ISFilter[0])
		sprintf(Signon, "user %s pass %d vers BPQ32 %s filter %s\r\n",
			APRSCall, ISPasscode, TextVerstring, ISFilter);
	else
		sprintf(Signon, "user %s pass %d vers BPQ32 %s\r\n",
			APRSCall, ISPasscode, TextVerstring);


	sprintf(PortString, "%d", ISPort);

	// get host info, make socket, and connect it

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(ISHost, PortString, &hints, &res);

	InputLen = sprintf(errmsg, "Connecting to APRS Host %s\r\n", ISHost);
	MonitorAPRSIS(errmsg, InputLen, FALSE);

	if (!res)
	{
		err = WSAGetLastError();
		InputLen = sprintf(errmsg, "APRS IS Resolve %s Failed Error %d\r\n", ISHost, err);
		MonitorAPRSIS(errmsg, InputLen, FALSE);

		return;					// Resolve failed
	
	}

	// Step thorough the list of hosts

	saveres = res;				// Save for free

	if (res->ai_next)			// More than one
	{
		int n = ISHostIndex;

		while (n && res->ai_next)
		{
			res = res->ai_next;
			n--;
		}

		if (n)
		{
			// We have run off the end of the list

			ISHostIndex = 0;	// Back to start
			res = saveres;
		}
		else
			ISHostIndex++;

	}

	getnameinfo(res->ai_addr, res->ai_addrlen, host, 256, serv, 256, 0);

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	if (sock == INVALID_SOCKET)
  	 	return; 
 
	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

	memcpy(work, res->ai_addr->sa_data, 4);

	Debugprintf("Trying  APRSIS Host %d.%d.%d.%d (%d) %s", work[0], work[1], work[2], work[3], ISHostIndex, host);
	
	if (connect(sock, res->ai_addr, res->ai_addrlen))
	{
		err=WSAGetLastError();

		//
		//	Connect failed
		//

#ifndef LINBPQ
		MySetWindowText(GetDlgItem(hConsWnd, IGATESTATE), "IGate State: Connect Failed");
#else
		printf("APRS Igate connect failed\n");
#endif
		err=WSAGetLastError();
		InputLen = sprintf(errmsg, "Connect Failed %s af %d Error %d \r\n", host, res->ai_family, err);
		MonitorAPRSIS(errmsg, InputLen, FALSE);

		freeaddrinfo(res);
		return;
	}

	freeaddrinfo(saveres);

	APRSISOpen = TRUE;

#ifndef LINBPQ
	MySetWindowText(GetDlgItem(hConsWnd, IGATESTATE), "IGate State: Connected");
#endif

	InputLen=recv(sock, Buffer, 500, 0);

	if (InputLen > 0)
	{
		Buffer[InputLen] = 0;
		Debugprintf(Buffer);
		MonitorAPRSIS(Buffer, InputLen, FALSE);
	}

	ISSend(sock, Signon, strlen(Signon), 0);
/*
	InputLen=recv(sock, Buffer, 500, 0);

	if (InputLen > 0)
	{
		Buffer[InputLen] = 0;
		Debugprintf(Buffer);
		MonitorAPRSIS(Buffer, InputLen, FALSE);
	}
 
	InputLen=recv(sock, Buffer, 500, 0);

	if (InputLen > 0)
	{
		Buffer[InputLen] = 0;
		Debugprintf(Buffer);
		MonitorAPRSIS(Buffer, InputLen, FALSE);
	}
*/
	while (InputLen > 0 && IGateEnabled)
	{
		InputLen = recv(sock, &APRSinMsg[inptr], 500 - inptr, 0);

		if (InputLen > 0)
		{
			inptr += InputLen;

			ptr = memchr(APRSinMsg, 0x0a, inptr);

			while (ptr != NULL)
			{
				ptr++;									// include lf
				len = ptr-(char *)APRSinMsg;	

				inptr -= len;						// bytes left
			
				// UIView server has a null before crlf

				if (*(ptr - 3) == 0)
				{
					*(ptr - 3) = 13;
					*(ptr - 2) = 10;
					*(ptr - 1) = 0;

					len --;
				}

				if (len < 300)							// Ignore if way too long
				{
					memcpy(&APRSMsg, APRSinMsg, len);	
					MonitorAPRSIS(APRSMsg, len, FALSE);
					APRSMsg[len - 2] = 0;
//					Debugprintf("%s", APRSMsg);

					ProcessAPRSISMsg(APRSMsg);
				}

				if (inptr > 0)
				{
					memmove(APRSinMsg, ptr, inptr);
					ptr = memchr(APRSinMsg, 0x0a, inptr);
				}
				else
					ptr = 0;

				if (inptr < 0)
					break;
			}
		}
	}

	closesocket(sock);

	APRSISOpen = FALSE;

	Debugprintf("BPQ32 APRS IS Thread Exited");

#ifndef LINBPQ
	if (IGateEnabled)
		SetDlgItemText(hConsWnd, IGATESTATE, "IGate State: Disconnected");
	else
		SetDlgItemText(hConsWnd, IGATESTATE, "IGate State: Disabled");
#endif
	ISDelayTimer = 30;		// Retry pretty quickly
	return;
}

VOID ProcessAPRSISMsg(char * APRSMsg)
{
	char * Payload;
	char * Source;
	char * Dest;
	char IGateCall[10] = "         ";
	char * ptr;
	char Message[255];
	PAPRSSTATIONRECORD MH;
	time_t NOW = time(NULL);
	char ISCopy[1024];
	struct STATIONRECORD * Station = NULL;
#ifdef WIN32
	struct _EXCEPTION_POINTERS exinfo;
	char EXCEPTMSG[80] = "";
#endif		

	if (APRSMsg[0] == '#')		// Comment
		return;

	// if APRS Appl is atttached, queue message to it

	strcpy(ISCopy, APRSMsg);

	GetSemaphore(&Semaphore, 12);

#ifdef WIN32

	strcpy(EXCEPTMSG, "ProcessAPRSISMsg");

	__try 
	{

	Station = DecodeAPRSISMsg(ISCopy);

	}
	#include "StdExcept.c"
	Debugprintf(APRSMsg);
	}
#else
	Station = DecodeAPRSISMsg(ISCopy);
#endif

	FreeSemaphore(&Semaphore);

//}WB4APR-14>APRS,RELAY,TCPIP,G9RXG*::G3NRWVVVV:Hi Ian{001
//KE7XO-2>hg,TCPIP*,qAC,T2USASW::G8BPQ-14 :Path - G8BPQ-14>APU25N
//IGATECALL>APRS,GATEPATH}FROMCALL>TOCALL,TCPIP,IGATECALL*:original packet data
	
	Payload = strchr(APRSMsg, ':');

	// Get call of originating Igate

	ptr = Payload;

	if (Payload == NULL)
		return;

	*(Payload++) = 0;

	while (ptr[0] != ',')
		ptr--;

	ptr++;

	if (strlen(ptr) > 9)
		return;

	memcpy(IGateCall, ptr, strlen(ptr));

	if (strstr(APRSMsg, ",qAS,") == 0)		// Findu generates invalid q construct
	{
		MH = LookupStation(IGateCall);
		if (MH)
		{
//			Debugprintf("Setting Igate Flag - %s:%s", APRSMsg, Payload);
			MH->IGate = TRUE;						// If we have seen this station on RF, set it as an Igate
		}
	}
	Source = APRSMsg;
	Dest = strchr(APRSMsg, '>');

	if (Dest == NULL)
		return;

	*(Dest++) = 0;				// Termainate Source
	ptr = strchr(Dest, ',');

	if (ptr)
		*ptr = 0;

	MH = UpdateHeard(Source, 0);

	MH->Station = Station;

	// See if we should gate to RF. 

	// Have we heard dest recently? (use the message dest (not ax.25 dest) - does this mean we only gate Messages?
	// Not if it is an Igate (it will get a copy direct)
	// Have we recently sent a message from this call - if so, we gate the next Position

	if (Payload[0] == ':')		// Message
	{
		char MsgDest[10];
		APRSSTATIONRECORD * STN;

		memcpy(MsgDest, &Payload[1], 9);
		MsgDest[9] = 0;

		if (strcmp(MsgDest, CallPadded) == 0) // to us
			return;

		STN = LookupStation(MsgDest);

		// Shouldn't we check DUP list, in case we have digi'ed this message directly?

		if (CheckforDups(Source, Payload, strlen(Payload)))
			return;

		if (STN && STN->Port && !STN->IGate && (NOW - STN->MHTIME) < GATETIMELIMIT) 
		{
			sprintf(Message, "}%s>%s,TCPIP,%s*:%s", Source, Dest, APRSCall, Payload);

			GetSemaphore(&Semaphore, 12);
//			SendAPRSMessage(Message, STN->Port);	
			FreeSemaphore(&Semaphore);

			MessageCount++;
			MH->LASTMSG = NOW;

			return;
		}
	}

	// Not a message. Only gate if have sent a message recently

	if ((NOW - MH->LASTMSG) < 900 && MH->Port)
	{
		sprintf(Message, "}%s>%s,TCPIP,%s*:%s", Source, Dest, APRSCall, Payload);
	
		GetSemaphore(&Semaphore, 12);
//		SendAPRSMessage(Message, -1);		// Send to all APRS Ports
		FreeSemaphore(&Semaphore);

		return;
	}

	// If Gate Local to RF is defined, and station is in range, Gate it

	if (GateLocal && Station)
	{
		if (Station->Object)
			Station = Station->Object;		// If Object Report, base distance on Object, not station
		
		if (Station->Lat != 0.0 && Station->Lon != 0.0 && Distance(Station->Lat, Station->Lon) < GateLocalDistance)
		{
			sprintf(Message, "}%s>%s,TCPIP,%s*:%s", Source, Dest, APRSCall, Payload);
			GetSemaphore(&Semaphore, 12);
			SendAPRSMessage(Message, -1);		// Send to all APRS Ports
			FreeSemaphore(&Semaphore);

			return;
		}
	}
}

APRSSTATIONRECORD * LookupStation(char * Call)
{
	APRSSTATIONRECORD * MH = MHDATA;
	int i;

	// We keep call in ascii format, as that is what we get from APRS-IS, and we have it in that form

	for (i = 0; i < HEARDENTRIES; i++)
	{
		if (memcmp(Call, MH->MHCALL, 9) == 0)
			return MH;

		MH++;
	}

	return NULL;
}

APRSSTATIONRECORD * UpdateHeard(UCHAR * Call, int Port)
{
	APRSSTATIONRECORD * MH = MHDATA;
	APRSSTATIONRECORD * MHBASE = MH;
	int i;
	time_t NOW = time(NULL);
	time_t OLDEST = NOW - MAXAGE;
	char CallPadded[10] = "         ";
	BOOL SaveIGate = FALSE;
	time_t SaveLastMsg = 0;

	// We keep call in ascii format, space padded, as that is what we get from APRS-IS, and we have it in that form

	// Make Sure Space Padded

	memcpy(CallPadded, Call, strlen(Call));

	for (i = 0; i < MAXHEARDENTRIES; i++)
	{
		if (memcmp(CallPadded, MH->MHCALL, 10) == 0)
		{
			// if from APRS-IS, only update if record hasn't been heard via RF
			
			if (Port == 0 && MH->Port) 
				return MH;					// Don't update RF with IS

			if (Port == MH->Port)
			{
				SaveIGate = MH->IGate;
				SaveLastMsg = MH->LASTMSG;
				goto DoMove;
			}
		}

		if (MH->MHCALL[0] == 0 || MH->MHTIME < OLDEST)		// Spare entry
			goto DoMove;

		MH++;
	}

	//	TABLE FULL AND ENTRY NOT FOUND - MOVE DOWN ONE, AND ADD TO TOP

	i = MAXHEARDENTRIES - 1;
		
	// Move others down and add at front
DoMove:
	if (i != 0)				// First
		memmove(MHBASE + 1, MHBASE, i * sizeof(APRSSTATIONRECORD));

	if (i >= HEARDENTRIES) 
	{
		char Status[80];
	
		HEARDENTRIES = i + 1;

		sprintf(Status, "IGATE Stats: Msgs %d  Local Stns %d", MessageCount , CountLocalStations());
#ifndef LINBPQ
		SetDlgItemText(hConsWnd, IGATESTATS, Status);
#endif
	}

	memcpy (MHBASE->MHCALL, CallPadded, 10);
	MHBASE->Port = Port;
	MHBASE->MHTIME = NOW;
	MHBASE->IGate = SaveIGate;
	MHBASE->LASTMSG = SaveLastMsg;

	return MHBASE;
}

int CountLocalStations()
{
	APRSSTATIONRECORD * MH = MHDATA;
	int i, n = 0;

	// We keep call in ascii format, as that is what we get from APRS-IS, and we have it in that form

	for (i = 0; i < HEARDENTRIES; i++)
	{
		if (MH->Port)			// DOn't count IS Stations
			n++;

		MH++;
	}

	return n;
}


BOOL CheckforDups(char * Call, char * Msg, int Len)
{
	// Primitive duplicate suppression - see if same call and text reeived in last few seconds
	
	time_t Now = time(NULL);
	time_t DupCheck = Now - DUPSECONDS;
	int i, saveindex = -1;
	char * ptr1;

	if (Len < 1)
		return TRUE;

	for (i = 0; i < MAXDUPS; i++)
	{
		if (DupInfo[i].DupTime < DupCheck)
		{
			// too old - use first if we need to save it 

			if (saveindex == -1)
			{
				saveindex = i;
			}

			if (DupInfo[i].DupTime == 0)		// Off end of used area
				break;

			continue;	
		}

		if ((Len == DupInfo[i].DupLen || (DupInfo[i].DupLen == 99 && Len > 99)) && memcmp(Call, DupInfo[i].DupUser, 7) == 0 && (memcmp(Msg, DupInfo[i].DupText, DupInfo[i].DupLen) == 0))
		{
			// Duplicate, so discard

			Msg[Len] = 0;
			ptr1 = strchr(Msg, 13);
			if (ptr1)
				*ptr1 = 0;

//			Debugprintf("Duplicate Message supressed %s", Msg);
			return TRUE;					// Duplicate
		}
	}

	// Not in list

	if (saveindex == -1)  // List is full
		saveindex = MAXDUPS - 1;	// Stick on end	

	DupInfo[saveindex].DupTime = Now;
	memcpy(DupInfo[saveindex].DupUser, Call, 7);

	if (Len > 99) Len = 99;

	DupInfo[saveindex].DupLen = Len;
	memcpy(DupInfo[saveindex].DupText, Msg, Len);

	return FALSE;
}

char * FormatAPRSMH(APRSSTATIONRECORD * MH)
 {
	 // Called from CMD.ASM

	struct tm * TM;
	static char MHLine[50];
	time_t szClock = MH->MHTIME;

	szClock = (time(NULL) - szClock);
	TM = gmtime(&szClock);

	sprintf(MHLine, "%-10s %d %.2d:%.2d:%.2d:%.2d %s\r",
		MH->MHCALL, MH->Port, TM->tm_yday, TM->tm_hour, TM->tm_min, TM->tm_sec, (MH->IGate) ? "IGATE" : "");

	return MHLine;
 }

// GPS Handling Code

void SelectSource(BOOL Recovering);
void DecodeRMC(char * msg, int len);

void PollGPSIn();


UINT GPSType = 0xffff;		// Source of Postion info - 1 = Phillips 2 = AIT1000. ffff = not posn message

int RecoveryTimer;			// Serial Port recovery

double PI = 3.1415926535;
double P2 = 3.1415926535 / 180;

double Latitude, Longtitude, SOG, COG, LatIncrement, LongIncrement;
double LastSOG = -1.0;

BOOL Check0183CheckSum(char * msg,int len)
{
	BOOL retcode=TRUE;
	char * ptr;
	UCHAR sum,xsum1,xsum2;

	sum=0;
	ptr=++msg;	//	Skip $

loop:

	if (*(ptr)=='*') goto eom;
	
	sum ^=*(ptr++);

	len--;

	if (len > 0) goto loop;

	return TRUE;		// No Checksum

eom:
	_strupr(ptr);

	xsum1=*(++ptr);
	xsum1-=0x30;
	if (xsum1 > 9) xsum1-=7;

	xsum2=*(++ptr);
	xsum2-=0x30;
	if (xsum2 > 9) xsum2-=7;

	xsum1=xsum1<<4;
	xsum1+=xsum2;

	return (xsum1==sum);
}

BOOL OpenGPSPort()
{
	struct PortInfo * portptr = &InPorts[0];

	// open COMM device

	portptr->hDevice = OpenCOMPort((VOID *)GPSPort, GPSSpeed, TRUE, TRUE, FALSE, 0);
				  
	if (portptr->hDevice == 0)
	{
		return FALSE;
	}

 	return TRUE;
}

void PollGPSIn()
{
	int len;
	char GPSMsg[2000] = "$GPRMC,061213.000,A,5151.5021,N,00056.8388,E,0.15,324.11,190414,,,A*6F";
	char * ptr;
	struct PortInfo * portptr;

	portptr = &InPorts[0];
	
	if (!portptr->hDevice)
		return;

	getgpsin:

// Comm Error - probably lost USB Port. Try closing and reopening after a delay

//			if (RecoveryTimer == 0)
//			{
//				RecoveryTimer = 100;			// 10 Secs
//				return;
//			}
//		}

		if (portptr->gpsinptr == 160)
			portptr->gpsinptr = 0;

		len = ReadCOMBlock(portptr->hDevice, &portptr->GPSinMsg[portptr->gpsinptr],
				160 - portptr->gpsinptr);

		if (len > 0)
		{
			portptr->gpsinptr+=len;

			ptr = memchr(portptr->GPSinMsg, 0x0a, portptr->gpsinptr);

			while (ptr != NULL)
			{
				ptr++;									// include lf
				len=ptr-(char *)&portptr->GPSinMsg;					
				memcpy(&GPSMsg,portptr->GPSinMsg,len);	

				GPSMsg[len] = 0;

				if (Check0183CheckSum(GPSMsg, len))
					if (memcmp(&GPSMsg[1], "GPRMC", 5) == 0)
						DecodeRMC(GPSMsg, len);	

				portptr->gpsinptr-=len;			// bytes left

				if (portptr->gpsinptr > 0 && *ptr == 0)
				{
					*ptr++;
					portptr->gpsinptr--;
				}

				if (portptr->gpsinptr > 0)
				{
					memmove(portptr->GPSinMsg,ptr, portptr->gpsinptr);
					ptr = memchr(portptr->GPSinMsg, 0x0a, portptr->gpsinptr);
				}
				else
					ptr=0;
			}
			
			goto getgpsin;
	}
	return;
}


void ClosePorts()
{
	if (InPorts[0].hDevice)
	{
		CloseCOMPort(InPorts[0].hDevice);
		InPorts[0].hDevice=0;
	}

	return;
}

void DecodeRMC(char * msg, int len)
{
	char * ptr1;
	char * ptr2;
	char TimHH[3], TimMM[3], TimSS[3];
	char OurSog[5], OurCog[4];
	char LatDeg[3], LonDeg[4];
	char NewLat[10] = "", NewLon[10] = "";
	struct STATIONRECORD * Stn1, * Stn2;

	char Day[3];

	ptr1 = &msg[7];
        
	len-=7;
	
	ptr2=(char *)memchr(ptr1,',',15);

	if (ptr2 == 0) return;	// Duff

	*(ptr2++)=0;

	memcpy(TimHH,ptr1,2);
	memcpy(TimMM,ptr1+2,2);
	memcpy(TimSS,ptr1+4,2);
	TimHH[2]=0;
	TimMM[2]=0;
	TimSS[2]=0;

	ptr1=ptr2;
	
	if (*(ptr1) != 'A') // ' Data Not Valid
	{
#ifndef LINBPQ
		SetDlgItemText(hConsWnd, IDC_GPS, "No GPS Fix");	
#endif
		return;
	}
        
	ptr1+=2;

	ptr2=(char *)memchr(ptr1,',',15);
		
	if (ptr2 == 0) return;	// Duff

	*(ptr2++)=0;
 
	memcpy(NewLat, ptr1, 7);
	memcpy(LatDeg, ptr1, 2);
	LatDeg[2]=0;
	Lat=atof(LatDeg) + (atof(ptr1+2)/60);
	
	if (*(ptr1+7) > '4') if (NewLat[6] < '9') NewLat[6]++;

	ptr1=ptr2;

	NewLat[7] = (*ptr1);
	if ((*ptr1) == 'S') Lat=-Lat;
	
	ptr1+=2;

	ptr2=(char *)memchr(ptr1,',',15);
		
	if (ptr2 == 0) return;	// Duff
	*(ptr2++)=0;

	memcpy(NewLon, ptr1, 8);
	
	memcpy(LonDeg,ptr1,3);
	LonDeg[3]=0;
	Lon=atof(LonDeg) + (atof(ptr1+3)/60);
       
	if (*(ptr1+8) > '4') if (NewLon[7] < '9') NewLon[7]++;

	ptr1=ptr2;

	NewLon[8] = (*ptr1);
	if ((*ptr1) == 'W') Lon=-Lon;

	// Now have a valid posn, so stop sending Undefined LOC Sysbol
	
	SYMBOL = CFGSYMBOL;
	SYMSET = CFGSYMSET;

	PosnSet = TRUE;

	Stn1  = (struct STATIONRECORD *)APRSStationMemory;		// Pass to App
	Stn1->Lat = Lat;
	Stn1->Lon = Lon;

	if (GPSOK == 0)
	{
#ifdef LINBPQ
		Debugprintf("GPS OK");
#else
		SetDlgItemText(hConsWnd, IDC_GPS, "GPS OK");
#endif
	}

	GPSOK = 30;	

	ptr1+=2;

	ptr2 = (char *)memchr(ptr1,',',30);
	
	if (ptr2 == 0) return;	// Duff

	*(ptr2++)=0;

	memcpy(OurSog, ptr1, 4);
	OurSog[4] = 0;

	ptr1=ptr2;

	ptr2 = (char *)memchr(ptr1,',',15);
	
	if (ptr2 == 0) return;	// Duff

	*(ptr2++)=0;

	memcpy(OurCog, ptr1, 3);
	OurCog[3] = 0;

	memcpy(Day,ptr2,2);
	Day[2]=0;

	SOG = atof(OurSog);
	COG = atof(OurCog);

	if (MobileBeaconInterval && (strcmp(NewLat, LAT) || strcmp(NewLon, LON)))
	{
		time_t NOW = time(NULL);

		if ((NOW - LastMobileBeacon) > MobileBeaconInterval)
		{
			LastMobileBeacon = NOW;
			SendBeacon(0, StatusMsg, FALSE, TRUE);
		}
	}

	strcpy(LAT, NewLat);
	strcpy(LON, NewLon);
}

VOID SendFilterCommand(char * Filter)
{
	char Msg[2000];
	int n;

	n = sprintf(Msg, ":%-9s:filter %s{1", "SERVER", Filter);

	PutAPRSMessage(Msg, n);

	n = sprintf(Msg, ":%-9s:filter?{1", "SERVER");
	PutAPRSMessage(Msg, n);
}


Dll VOID APIENTRY APRSConnect(char * Call, char * Filter)
{
	// Request APRS Data from Switch (called by APRS Applications)

	APRSApplConnected = TRUE;
	APRSWeb = TRUE;

	strcpy(APPLFilter, Filter);

	if (APPLFilter[0])
	{
		strcpy(ISFilter, APPLFilter);
		SendFilterCommand(ISFilter);
	}
	strcpy(Call, CallPadded);
}

Dll VOID APIENTRY APRSDisconnect()
{
	// Stop requesting APRS Data from Switch (called by APRS Applications)

	UINT * buffptr;

	APRSApplConnected = FALSE;
	APRSWeb =FALSE;


	strcpy(ISFilter, NodeFilter);

	SendFilterCommand(ISFilter);

	while (APPL_Q)
	{
		buffptr = Q_REM(&APPL_Q);
		ReleaseBuffer(buffptr);
	}

}

Dll BOOL APIENTRY GetAPRSFrame(char * Frame, char * Call)
{
	// Request APRS Data from Switch (called by APRS Applications)

	UINT * buffptr;
#ifdef bpq32
	struct _EXCEPTION_POINTERS exinfo;
#endif

	GetSemaphore(&Semaphore, 10);
	{
		if (APPL_Q)
		{
			buffptr = Q_REM(&APPL_Q);

			memcpy(Call, (char *)&buffptr[2], 12);
			strcpy(Frame, (char *)&buffptr[5]);

			ReleaseBuffer(buffptr);
			FreeSemaphore(&Semaphore);
			return TRUE;
		}
	}

	FreeSemaphore(&Semaphore);

	return FALSE;
}

Dll BOOL APIENTRY PutAPRSFrame(char * Frame, int Len, int Port)
{
	// Called from BPQAPRS App
	// Message has to be queued so it can be sent by Timer Process (IS sock is not valid in this context)

	UINT * buffptr;

	GetSemaphore(&Semaphore, 11);

	buffptr = GetBuff();

	if (buffptr)
	{
		buffptr[1] = ++Len;			// Len doesn't include Null
		memcpy(&buffptr[3], Frame, Len);
		C_Q_ADD(&APPLTX_Q, buffptr);
	}

	buffptr[2] = Port;				// Pass to SendAPRSMessage();

	FreeSemaphore(&Semaphore);

	return TRUE;
}

Dll BOOL APIENTRY PutAPRSMessage(char * Frame, int Len)
{
	// Called from BPQAPRS App
	// Message has to be queued so it can be sent by Timer Process (IS sock is not valid in this context)

	UINT * buffptr;

	GetSemaphore(&Semaphore, 11);

	buffptr = GetBuff();

	if (buffptr)
	{
		buffptr[1] = ++Len;			// Len doesn't include Null
		memcpy(&buffptr[3], Frame, Len);
		C_Q_ADD(&APPLTX_Q, buffptr);
	}

	buffptr[2] = -1;				// Pass to SendAPPLAPRSMessagee();

	FreeSemaphore(&Semaphore);

	return TRUE;
}

BOOL SendAPPLAPRSMessage(char * Frame)
{
	// Send an APRS Message from Appl Queue. If call has been heard,
	// send to the port it was heard on,
	// otherwise send to all ports (including IS). Messages to SERVER only go to IS

	APRSSTATIONRECORD * STN;
	char ToCall[10] = "";
	
	memcpy(ToCall, &Frame[1], 9);

	if (_stricmp(ToCall, "SERVER   ") == 0)
	{
		SendAPRSMessage(Frame, 0);			// IS
		return TRUE;
	}

	MessageCount++;							// Don't include SERVER messages in count

	STN = LookupStation(ToCall);

	if (STN)
		SendAPRSMessage(Frame, STN->Port);
	else
	{
		SendAPRSMessage(Frame, -1);			// All RF ports
		SendAPRSMessage(Frame, 0);			// IS
	}
	
	return TRUE;
}

Dll BOOL APIENTRY GetAPRSLatLon(double * PLat,  double * PLon)
{
	*PLat = Lat;
	*PLon = Lon;

	return GPSOK;
}

Dll BOOL APIENTRY GetAPRSLatLonString(char * PLat,  char * PLon)
{
	strcpy(PLat, LAT);
	strcpy(PLon, LON);

	return GPSOK;
}

// Code to support getting GPS from Andriod Device running BlueNMEA


#define SD_BOTH         0x02

static char Buffer[8192];
int SavedLen = 0;


static VOID ProcessReceivedData(SOCKET TCPSock)
{
	char UDPMsg[8192];

	int len = recv(TCPSock, &Buffer[SavedLen], 8100 - SavedLen, 0);

	char * ptr;
	char * Lastptr;

	if (len <= 0)
	{
		closesocket(TCPSock);
		BlueNMEAOK = FALSE;
		return;
	}

	len += SavedLen;
	SavedLen = 0;

	ptr = Lastptr = Buffer;

	Buffer[len] = 0;

	while (len > 0)
	{
		ptr = strchr(Lastptr, 10);

		if (ptr)
		{
			int Len = ptr - Lastptr;
		
			memcpy(UDPMsg, Lastptr, Len);
			UDPMsg[Len++] = 13;
			UDPMsg[Len++] = 10;
			UDPMsg[Len++] = 0;

			if (!Check0183CheckSum(UDPMsg, Len))
			{
				Debugprintf("Checksum Error %s", UDPMsg);
			}
			else
			{			
				if (memcmp(UDPMsg, "$GPRMC", 6) == 0)
					DecodeRMC(UDPMsg, Len);

			}
			Lastptr = ptr + 1;
			len -= Len;
		}
		else
			SavedLen = len;
	}
}

static VOID TCPConnect()
{
	int err, ret;
	u_long param=1;
	BOOL bcopt=TRUE;
	fd_set readfs;
	fd_set errorfs;
	struct timeval timeout;
	SOCKADDR_IN sinx; 
	struct sockaddr_in destaddr;
	SOCKET TCPSock;
	int addrlen=sizeof(sinx);

	if (HostName[0] == 0)
		return;

	destaddr.sin_addr.s_addr = inet_addr(HostName);
	destaddr.sin_family = AF_INET;
	destaddr.sin_port = htons(4352);

	TCPSock = socket(AF_INET,SOCK_STREAM,0);

	if (TCPSock == INVALID_SOCKET)
	{
  	 	return; 
	}
 
	setsockopt (TCPSock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt, 4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(TCPSock, (LPSOCKADDR) &sinx, addrlen) != 0 )
	{
		//
		//	Bind Failed
		//
	
		closesocket(TCPSock);

  	 	return; 
	}

	if (connect(TCPSock,(LPSOCKADDR) &destaddr, sizeof(destaddr)) == 0)
	{
		//
		//	Connected successful
		//
	}
	else
	{
		err=WSAGetLastError();
#ifdef LINBPQ
   		printf("Connect Failed for BlueNMEA socket - error code = %d\n", err);
#else
   		Debugprintf("Connect Failed for BlueNMEA socket - error code = %d", err);
#endif		
		closesocket(TCPSock);
		return;
	}

	BlueNMEAOK = TRUE;

	while (TRUE)
	{
		FD_ZERO(&readfs);	
		FD_ZERO(&errorfs);

		FD_SET(TCPSock,&readfs);
		FD_SET(TCPSock,&errorfs);

		timeout.tv_sec = 60;
		timeout.tv_usec = 0;				// We should get messages more frequently that this

		ret = select(TCPSock + 1, &readfs, NULL, &errorfs, &timeout);
		
		if (ret == SOCKET_ERROR)
		{
			goto Lost;
		}
		if (ret > 0)
		{
			//	See what happened

			if (FD_ISSET(TCPSock, &readfs))
			{
				ProcessReceivedData(TCPSock);			
			}
								
			if (FD_ISSET(TCPSock, &errorfs))
			{
Lost:				
#ifdef LINBPQ
				printf("BlueNMEA Connection lost\n");
#endif			
				closesocket(TCPSock);
				BlueNMEAOK = FALSE;;
				return;
			}
		}
		else
		{
			// 60 secs without data. Shouldn't happen

			shutdown(TCPSock, SD_BOTH);
			Sleep(100);
			closesocket(TCPSock);
			BlueNMEAOK = FALSE;
			return;
		}
	}
}

// Code Moved from APRS Application

//
// APRS Mapping and Messaging App for BPQ32 Switch.
//


VOID APIENTRY APRSConnect(char * Call, char * Filter);
VOID APIENTRY APRSDisconnect();
BOOL APIENTRY GetAPRSFrame(char * Frame, char * Call);
BOOL APIENTRY PutAPRSFrame(char * Frame, int Len, int Port);
BOOL APIENTRY PutAPRSMessage(char * Frame, int Len);
BOOL APIENTRY GetAPRSLatLon(double * PLat,  double * PLon);
BOOL APIENTRY GetAPRSLatLonString(char * PLat,  char * PLon);
VOID APIENTRY APISendBeacon();


int NewLine(HWND hWnd);
VOID	ProcessBuff(HWND hWnd, MESSAGE * buff,int len,int stamp);
int TogglePort(HWND hWnd, int Item, int mask);
VOID SendFrame(UCHAR * buff, int txlen);
int	KissEncode(UCHAR * inbuff, UCHAR * outbuff, int len);
int	KissDecode(UCHAR * inbuff, int len);
//void UpdateStation(char * Call, char * Path, char * Comment, double V_Lat, double V_Lon, double V_SOG, double V_COG, int iconRow, int iconCol);
VOID FindStationsByPixel(int MouseX, int MouseY);
void RefreshStation(struct STATIONRECORD * ptr);
void RefreshStationList();
void RefreshStationMap();
BOOL DecodeLocationString(UCHAR * Payload, struct STATIONRECORD * Station);
VOID Decode_MIC_E_Packet(char * Payload, struct STATIONRECORD * Station);
BOOL GetLocPixels(double Lat, double Lon, int * X, int * Y);
VOID APRSPoll();
VOID OSMThread();
VOID ResolveThread();
VOID RefreshTile(char * FN, int Zoom, int x, int y);
VOID ProcessMessage(char * Payload, struct STATIONRECORD * Station);
VOID APRSSecTimer();
double Distance(double laa, double loa);
double Bearing(double laa, double loa);

BOOL CreatePipeThread();

VOID SendWeatherBeacon();
VOID DecodeWXPortList();

	
VOID DecodeWXReport(struct APRSConnectionInfo * sockptr, char * WX)
{
	UCHAR * ptr = strchr(WX, '_');
	char Type;
	int Val;

	if (ptr == 0)
		return;

	sockptr->WindDirn = atoi(++ptr);
	ptr += 4;
	sockptr->WindSpeed = atoi(ptr);
	ptr += 3;
WXLoop:

	Type = *(ptr++);

	if (*ptr =='.')	// Missing Value
	{
		while (*ptr == '.')
			ptr++;

		goto WXLoop;
	}

	Val = atoi(ptr);

	switch (Type)
	{
	case 'c': // = wind direction (in degrees).	
		
		sockptr->WindDirn = Val;
		break;
	
	case 's': // = sustained one-minute wind speed (in mph).
	
		sockptr->WindSpeed = Val;
		break;
	
	case 'g': // = gust (peak wind speed in mph in the last 5 minutes).
	
		sockptr->WindGust = Val;
		break;

	case 't': // = temperature (in degrees Fahrenheit). Temperatures below zero are expressed as -01 to -99.
	
		sockptr->Temp = Val;
		break;

	case 'r': // = rainfall (in hundredths of an inch) in the last hour.
		
		sockptr->RainLastHour = Val;
		break;

	case 'p': // = rainfall (in hundredths of an inch) in the last 24 hours.

		sockptr->RainLastDay = Val;
		break;

	case 'P': // = rainfall (in hundredths of an inch) since midnight.

		sockptr->RainToday = Val;
		break;

	case 'h': // = humidity (in %. 00 = 100%).
	
		sockptr->Humidity = Val;
		break;

	case 'b': // = barometric pressure (in tenths of millibars/tenths of hPascal).

		sockptr->Pressure = Val;
		break;

	default:

		return;
	}
	while(isdigit(*ptr))
	{
		ptr++;
	}

	if (*ptr != ' ')
		goto WXLoop;
}

static char HeaderTemplate[] = "Accept: */*\r\nHost: %s\r\nConnection: close\r\nContent-Length: 0\r\nUser-Agent: BPQ32(G8BPQ)\r\n\r\n";
//char Header[] = "Accept: */*\r\nHost: tile.openstreetmap.org\r\nConnection: close\r\nContent-Length: 0\r\nUser-Agent: BPQ32(G8BPQ)\r\n\r\n";

char APRSMsg[300];

Dll struct STATIONRECORD *  APIENTRY APPLFindStation(char * Call, BOOL AddIfNotFount)
{
	//	Called from APRS Appl

	struct STATIONRECORD * Stn;

	GetSemaphore(&Semaphore, 12);
	Stn = FindStation(Call, AddIfNotFount)	;		
	FreeSemaphore(&Semaphore);

	return Stn;
}

struct STATIONRECORD * FindStation(char * Call, BOOL AddIfNotFount)
{
	int i = 0;
	struct STATIONRECORD * find;
	struct STATIONRECORD * ptr;
	struct STATIONRECORD * last = NULL;
	int sum = 0;

	if (APRSActive == 0 || StationRecords == 0)
		return FALSE;

	if (strlen(Call) > 9)
	{
		Debugprintf("APRS Call too long %s", Call);
		Call[9] = 0;
	}

	find = *StationRecords;
	while(find)
	{
		if (strlen(find->Callsign) > 9)
		{
			Debugprintf("APRS Call in Station List too long %s", find->Callsign);
			find->Callsign[9] = 0;
		}

	    if (strcmp(find->Callsign, Call) == 0)
			return find;

		last = find;
		find = find->Next;
		i++;
	}
 
	//   Not found - add on end

	if (AddIfNotFount)
	{
		// Get first from station record pool
		
		ptr = StationRecordPool;
		
		if (ptr)
		{
			StationRecordPool = ptr->Next;	// Unchain
			StationCount++;
		}
		else
		{
			//	Get First from Stations

			ptr = *StationRecords;
			if (ptr)
				*StationRecords = ptr->Next;
		}

		if (ptr == NULL) return NULL;

		memset(ptr, 0, sizeof(struct STATIONRECORD));
	
//		EnterCriticalSection(&Crit);

		if (*StationRecords == NULL)
			*StationRecords = ptr;
		else
			last->Next = ptr;

//		LeaveCriticalSection(&Crit);

		//	Debugprintf("APRS Add Stn %s Station Count = %d", Call, StationCount);
       
		strcpy(ptr->Callsign, Call);
		ptr->TimeAdded = time(NULL);
		ptr->Index = i;
		ptr->NoTracks = DefaultNoTracks;

		for (i = 0; i < 9; i++)
			sum += Call[i];

		sum %= 20;

		ptr->TrackColour = sum;
		ptr->Moved = TRUE;

		return ptr;
	}
	else
		return NULL;
}

struct STATIONRECORD * ProcessRFFrame(char * Msg, int len)
{
	char * Payload;
	char * Path = NULL;
	char * Comment = NULL;
	char * Callsign;
	char * ptr;
	int Port = 0;

	struct STATIONRECORD * Station = NULL;

	Msg[len - 1] = 0;

//	Debugprintf("RF Frame %s", Msg);

	Msg += 10;				// Skip Timestamp
	
	Payload = strchr(Msg, ':');			// End of Address String

	if (Payload == NULL)
	{
		Debugprintf("Invalid Msg %s", Msg);
		return Station;
	}

	ptr = strstr(Msg, "Port=");

	if (ptr)
		Port = atoi(&ptr[5]);

	Payload++;

	if (*Payload != 0x0d)
		return Station;

	*Payload++ = 0;

	Callsign = Msg;

	Path = strchr(Msg, '>');

	if (Path == NULL)
	{
		Debugprintf("Invalid Meader %s", Msg);
		return Station;
	}

	*Path++ = 0;

	ptr = strchr(Path, ' ');

	if (ptr)
		*ptr = 0;

	// Look up station - create a new one if not found

	Station = FindStation(Callsign, TRUE);
	
	strcpy(Station->Path, Path);
	strcpy(Station->LastPacket, Payload);
	Station->LastPort = Port;

	DecodeAPRSPayload(Payload, Station);
	Station->TimeLastUpdated = time(NULL);

	return Station;
}


/*
2E0AYY>APU25N,TCPIP*,qAC,AHUBSWE2:=5105.18N/00108.19E-Paul in Folkestone Kent {UIV32N}
G0AVP-12>APT310,MB7UC*,WIDE3-2,qAR,G3PWJ:!5047.19N\00108.45Wk074/000/Paul mobile
G0CJM-12>CQ,TCPIP*,qAC,AHUBSWE2:=/3&R<NDEp/  B>io94sg
M0HFC>APRS,WIDE2-1,qAR,G0MNI:!5342.83N/00013.79W# Humber Fortress ARC Look us up on QRZ
G8WVW-3>APTT4,WIDE1-1,WIDE2-1,qAS,G8WVW:T#063,123,036,000,000,000,00000000
*/


struct STATIONRECORD * DecodeAPRSISMsg(char * Msg)
{
	char * Payload;
	char * Path = NULL;
	char * Comment = NULL;
	char * Callsign;
	struct STATIONRECORD * Station = NULL;

//	Debugprintf(Msg);
		
	Payload = strchr(Msg, ':');			// End of Address String

	if (Payload == NULL)
	{
		Debugprintf("Invalid Msg %s", Msg);
		return Station;
	}

	*Payload++ = 0;

	Callsign = Msg;

	Path = strchr(Msg, '>');

	if (Path == NULL)
	{
		Debugprintf("Invalid Msg %s", Msg);
		return Station;
	}

	*Path++ = 0;

	// Look up station - create a new one if not found

	if (strlen(Callsign) > 11)
	{
		Debugprintf("Invalid Msg %s", Msg);
		return Station;
	}

	Station = FindStation(Callsign, TRUE);
	
	strcpy(Station->Path, Path);
	strcpy(Station->LastPacket, Payload);
	Station->LastPort = 0;

	DecodeAPRSPayload(Payload, Station);
	Station->TimeLastUpdated = time(NULL);

	return Station;
}

double Cube91 = 91.0 * 91.0 * 91.0;
double Square91 = 91.0 * 91.0;

BOOL DecodeLocationString(UCHAR * Payload, struct STATIONRECORD * Station)
{
	UCHAR SymChar;
	char SymSet;
	char NS;
	char EW;
	double NewLat, NewLon;
	char LatDeg[3], LonDeg[4];
	char save;

	// Compressed has first character not a digit (it is symbol table)

	// /YYYYXXXX$csT

	if (Payload[0] == '!')
		return FALSE;					// Ultimeter 2000 Weather Station

	if (!isdigit(*Payload))
	{
		int C, S;
		
		SymSet = *Payload;
		SymChar = Payload[9];

		NewLat = 90.0 - ((Payload[1] - 33) * Cube91 + (Payload[2] - 33) * Square91 +
			(Payload[3] - 33) * 91.0 + (Payload[4] - 33)) / 380926.0;

		Payload += 4;
				
		NewLon = -180.0 + ((Payload[1] - 33) * Cube91 + (Payload[2] - 33) * Square91 +
			(Payload[3] - 33) * 91.0 + (Payload[4] - 33)) / 190463.0;

		C = Payload[6] - 33;

		if (C >= 0 && C < 90 )
		{
			S = Payload[7] - 33;

			Station->Course = C * 4;
			Station->Speed = (pow(1.08, S) - 1) * 1.15077945;	// MPH; 
		}



	}
	else
	{
		// Standard format ddmm.mmN/dddmm.mmE?

		NS = Payload[7] & 0xdf;		// Mask Lower Case Bit
		EW = Payload[17] & 0xdf;

		SymSet = Payload[8];
		SymChar = Payload[18];

		memcpy(LatDeg, Payload,2);
		LatDeg[2]=0;
		NewLat = atof(LatDeg) + (atof(Payload+2) / 60);
       
		if (NS == 'S')
			NewLat = -NewLat;
		else
			if (NS != 'N')
				return FALSE;

		memcpy(LonDeg,Payload + 9, 3);

		if (Payload[22] == '/')
		{
			Station->Course = atoi(Payload + 19);
			Station->Speed = atoi(Payload + 23);
		}

		LonDeg[3]=0;

		save = Payload[17];
		Payload[17] = 0;
		NewLon = atof(LonDeg) + (atof(Payload+12) / 60);
		Payload[17] = save;
		
		if (EW == 'W')
			NewLon = -NewLon;
		else
			if (EW != 'E')
				return FALSE;
	}

	if (Station->Lat != NewLat || Station->Lon != NewLon)
	{
		time_t NOW = time(NULL);
		time_t Age = NOW - Station->TimeLastTracked;

		if (Age > 15)				// Don't update too often
		{
			// Add to track

			Station->TimeLastTracked = NOW;

//			if (memcmp(Station->Callsign, "ISS ", 4) == 0)
//				Debugprintf("%s %s %s ",Station->Callsign, Station->Path, Station->LastPacket);

			Station->LatTrack[Station->Trackptr] = NewLat;
			Station->LonTrack[Station->Trackptr] = NewLon;
			Station->TrackTime[Station->Trackptr] = NOW;

			Station->Trackptr++;
			Station->Moved = TRUE;

			if (Station->Trackptr == TRACKPOINTS)
				Station->Trackptr = 0;
		}

		Station->Lat = NewLat;
		Station->Lon = NewLon;	
	}

	Station->Symbol = SymChar;

	if (SymChar > ' ' && SymChar < 0x7f)
		SymChar -= '!';
	else
		SymChar = 0;

	Station->IconOverlay = 0;

	if ((SymSet >= '0' && SymSet <= '9') || (SymSet >= 'A' && SymSet <= 'Z'))
	{
		SymChar += 96;
		Station->IconOverlay = SymSet;
	}
	else
		if (SymSet == '\\')
			SymChar += 96;

	Station->iconRow = SymChar >> 4;
	Station->iconCol = SymChar & 15;

	return TRUE;
}

VOID DecodeAPRSPayload(char * Payload, struct STATIONRECORD * Station)
{
	char * TimeStamp;
	char * ObjName;
	char ObjState;
	struct STATIONRECORD * Object;
	BOOL Item = FALSE;
	char * ptr;
	char * Callsign;
	char * Path;
	char * Msg;
	struct STATIONRECORD * TPStation;
	int msgLen;
	unsigned char APIMsg[512];

	Station->Object = NULL;

	switch(*Payload)
	{
	case '`':
	case 0x27:					// '
	case 0x1c:
	case 0x1d:					// MIC-E

		Decode_MIC_E_Packet(Payload, Station);
		return;

	case '$':					// NMEA
		break;

	case ')':					// Item	

//		Debugprintf("%s %s %s", Station->Callsign, Station->Path, Payload);

		Item = TRUE;
		ObjName = ptr = Payload + 1;

		while (TRUE)
		{
			ObjState = *ptr;
			if (ObjState == 0)
				return;					// Corrupt

			if (ObjState == '!' || ObjState == '_')	// Item Terminator
				break;

			ptr++;
		}

		*ptr = 0;						// Terminate Name

		Object = FindStation(ObjName, TRUE);
		Object->ObjState = *ptr++ = ObjState;

		strcpy(Object->Path, Station->Callsign);
		strcat(Object->Path, ">");
		if (Object == Station)
		{
			char Temp[256];
			strcpy(Temp, Station->Path);
			strcat(Object->Path, Temp);
			Debugprintf("item is station %s", Payload);
		}
		else
			strcat(Object->Path, Station->Path);

		strcpy(Object->LastPacket, Payload);

		if (ObjState != '_')		// Deleted Objects may have odd positions
			DecodeLocationString(ptr, Object);

		Object->TimeLastUpdated = time(NULL);
		Station->Object = Object;
		return;


	case ';':					// Object

		ObjName = Payload + 1;
		ObjState = Payload[10];	// * Live, _Killed

		Payload[10] = 0;
		Object = FindStation(ObjName, TRUE);
		Object->ObjState = Payload[10] = ObjState;

		strcpy(Object->Path, Station->Callsign);
		strcat(Object->Path, ">");
		if (Object == Station)
		{
			char Temp[256];
			strcpy(Temp, Station->Path);
			strcat(Object->Path, Temp);
			Debugprintf("Object is station %s", Payload);
		}
		else
			strcat(Object->Path, Station->Path);


		strcpy(Object->LastPacket, Payload);

		TimeStamp = Payload + 11;

		if (ObjState != '_')		// Deleted Objects may have odd positions
			DecodeLocationString(Payload + 18, Object);
		
		Object->TimeLastUpdated = time(NULL);
		Station->Object = Object;
		return;

	case '@':
	case '/':					// Timestamp, No Messaging

		TimeStamp = ++Payload;
		Payload += 6;

	case '=':
	case '!':

		Payload++;
	
		DecodeLocationString(Payload, Station);

		if (Station->Symbol == '_')		// WX
		{
			if (strlen(Payload) > 50)
				strcpy(Station->LastWXPacket, Payload);
		}
		return;	

	case '>':				// Status

		strcpy(Station->Status, &Payload[1]);

	case '<':				// Capabilities
	case '_':				// Weather
	case 'T':				// Telemetry

		break;

	case ':':				// Message

		

#ifdef LINBPQ
#ifndef WIN32
		
		// if Liunx, Pass to Messaging APP - station pointer, then Message

		memcpy(APIMsg, &Station, 4);
		strcpy(&APIMsg[4], Payload);
		msgLen = strlen(Payload) + 5;
		sendto(sfd, APIMsg, msgLen, 0, (struct sockaddr *) &peer_addr, sizeof(struct sockaddr_un));
#endif
#else
		if (APRSApplConnected)
		{
			// Make sure we don't have too many queued (Appl could have crashed)
			
			UINT * buffptr;

			if (C_Q_COUNT(&APPL_Q) > 50)
				buffptr = Q_REM(&APPL_Q);
			else
				buffptr = GetBuff();
			
			if (buffptr)
			{
				buffptr[1] = 0;
				memcpy(&buffptr[2], Station->Callsign, 12);
				strcpy((char *)&buffptr[5], Payload);
				C_Q_ADD(&APPL_Q, buffptr);
			}
		}

#endif
		break;

	case '}':			// Third Party Header
			
		// Process Payload as a new message

		// }GM7HHB-9>APDR12,TCPIP,MM1AVR*:=5556.62N/00303.55W>204/000/A=000213 http://www.dstartv.com

		Callsign = Msg = &Payload[1];
		Path = strchr(Msg, '>');

		if (Path == NULL)
			return;

		*Path++ = 0;

		Payload = strchr(Path, ':');

		if (Payload == NULL)
			return;

		*(Payload++) = 0;

		// Check Dup Filter

		if (CheckforDups(Callsign, Payload, strlen(Payload)))
			return;

		// Look up station - create a new one if not found

		TPStation = FindStation(Callsign, TRUE);
	
		strcpy(TPStation->Path, Path);
		strcpy(TPStation->LastPacket, Payload);
		TPStation->LastPort = 0;					// Heard on RF, but info is from IS

		DecodeAPRSPayload(Payload, TPStation);
		TPStation->TimeLastUpdated = time(NULL);

		return;

	default:
//		Debugprintf("%s %s %s", Station->Callsign, Station->Path, Payload);
		return;
	}
}

// Convert MIC-E Char to Lat Digit (offset by 0x30)
//				  0123456789      @ABCDEFGHIJKLMNOPQRSTUVWXYZ				
char MicELat[] = "0123456789???????0123456789  ???0123456789 " ;

char MicECode[]= "0000000000???????111111111110???22222222222" ;


VOID Decode_MIC_E_Packet(char * Payload, struct STATIONRECORD * Station)
{
	// Info is encoded in the Dest Addr (in Station->Path) as well as Payload. 
	// See APRS Spec for full details

	char Lat[10];		// DDMMHH
	char LatDeg[3];
	char * ptr;
	char c;
	int i, n;
	int LonDeg, LonMin;
	BOOL LonOffset = FALSE;
	char NS = 'S';
	char EW = 'E';
	UCHAR SymChar, SymSet;
	double NewLat, NewLon;
	int SP, DC, SE;				// Course/Speed Encoded
	int Course, Speed;

	// Make sure packet is long enough to have an valid address

 	if (strlen(Payload) < 9)
		return;

	ptr = &Station->Path[0];

	for (i = 0; i < 6; i++)
	{
		n = (*(ptr++)) - 0x30;
		c = MicELat[n];

		if (c == '?')			// Illegal
			return;

		if (c == ' ')
			c = '0';			// Limited Precision
		
		Lat[i] = c;

	}

	Lat[6] = 0;

	if (Station->Path[3] > 'O')
		NS = 'N';

	if (Station->Path[5] > 'O')
		EW = 'W';

	if (Station->Path[4] > 'O')
		LonOffset = TRUE;

	n = Payload[1] - 28;			// Lon Degrees S9PU0T,WIDE1-1,WIDE2-2,qAR,WB9TLH-15:`rB0oII>/]"6W}44

	if (LonOffset)
		n += 100;

	if (n > 179 && n < 190)
		n -= 80;
	else
	if (n > 189 && n < 200)
		n -= 190;

	LonDeg = n;

/*
	To decode the longitude degrees value:
1. subtract 28 from the d+28 value to obtain d.
2. if the longitude offset is +100 degrees, add 100 to d.
3. subtract 80 if 180 ˜ d ˜ 189
(i.e. the longitude is in the range 100–109 degrees).
4. or, subtract 190 if 190 ˜ d ˜ 199.
(i.e. the longitude is in the range 0–9 degrees).
*/

	n = Payload[2] - 28;			// Lon Mins

	if (n > 59)
		n -= 60;

	LonMin = n;

	n = Payload[3] - 28;			// Lon Mins/100;

//1. subtract 28 from the m+28 value to obtain m.
//2. subtract 60 if m ™ 60.
//(i.e. the longitude minutes is in the range 0–9).


	memcpy(LatDeg, Lat, 2);
	LatDeg[2]=0;
	
	NewLat = atof(LatDeg) + (atof(Lat+2) / 6000.0);
       
	if (NS == 'S')
		NewLat = -NewLat;

	NewLon = LonDeg + LonMin / 60.0 + n / 6000.0;
       
	if (EW == 'W')				// West
		NewLon = -NewLon;

	SP = Payload[4] - 28;
	DC = Payload[5] - 28;
	SE = Payload[6] - 28;		// Course 100 and 10 degs

	Speed = DC / 10;		// Quotient = Speed Units
	Course = DC - (Speed * 10);	// Remainder = Course Deg/100

	Course = SE + (Course * 100);

	Speed += SP * 10;

	if (Speed >= 800)
		Speed -= 800;

	if (Course >= 400)
		Course -= 400;

	Station->Course = Course;
	Station->Speed = Speed * 1.15077945;	// MPH

//	Debugprintf("MIC-E Course/Speed %s %d %d", Station->Callsign, Course, Speed);

	if (Station->Lat != NewLat || Station->Lon != NewLon)
	{
		time_t NOW = time(NULL);
		time_t Age = NOW - Station->TimeLastUpdated;

		if (Age > 15)				// Don't update too often
		{
			// Add to track

//			if (memcmp(Station->Callsign, "ISS ", 4) == 0)
//				Debugprintf("%s %s %s ",Station->Callsign, Station->Path, Station->LastPacket);

			Station->LatTrack[Station->Trackptr] = NewLat;
			Station->LonTrack[Station->Trackptr] = NewLon;
			Station->TrackTime[Station->Trackptr] = NOW;

			Station->Trackptr++;
			Station->Moved = TRUE;

			if (Station->Trackptr == TRACKPOINTS)
				Station->Trackptr = 0;
		}

		Station->Lat = NewLat;
		Station->Lon = NewLon;
	}


	SymChar = Payload[7];			// Symbol
	SymSet = Payload[8];			// Symbol

	SymChar -= '!';

	Station->IconOverlay = 0;

	if ((SymSet >= '0' && SymSet <= '9') || (SymSet >= 'A' && SymSet <= 'Z'))
	{
		SymChar += 96;
		Station->IconOverlay = SymSet;
	}
	else
		if (SymSet == '\\')
			SymChar += 96;

	Station->iconRow = SymChar >> 4;
	Station->iconCol = SymChar & 15;

	return;

}

/*

INT_PTR CALLBACK ChildDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
//	This processes messages from controls on the tab subpages
	int Command;

//	int retCode, disp;
//	char Key[80];
//	HKEY hKey;
//	BOOL OK;
//	OPENFILENAME ofn;
//	char Digis[100];

	int Port = PortNum[CurrentPage];

	switch (message)
	{
	case WM_NOTIFY:

        switch (((LPNMHDR)lParam)->code)
        {
		case TCN_SELCHANGE:
			 OnSelChanged(hDlg);
				 return TRUE;
         // More cases on WM_NOTIFY switch.
		case NM_CHAR:
			return TRUE;
        }

       break;
	case WM_INITDIALOG:
		OnChildDialogInit( hDlg);
		return (INT_PTR)TRUE;

	case WM_CTLCOLORDLG:

        return (LONG)bgBrush;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, RGB(0, 0, 0));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LONG)bgBrush;
    }


	case WM_COMMAND:

		Command = LOWORD(wParam);

		if (Command == 2002)
			return TRUE;

		switch (Command)
		{
/*			case IDC_FILE:

			memset(&ofn, 0, sizeof (OPENFILENAME));
			ofn.lStructSize = sizeof (OPENFILENAME);
			ofn.hwndOwner = hDlg;
			ofn.lpstrFile = &FN[Port][0];
			ofn.nMaxFile = 250;
			ofn.lpstrTitle = "File to send as beacon";
			ofn.lpstrInitialDir = GetBPQDirectory();

			if (GetOpenFileName(&ofn))
				SetDlgItemText(hDlg, IDC_FILENAME, &FN[Port][0]);

			break;


		case IDOK:

			GetDlgItemText(hDlg, IDC_UIDEST, &UIDEST[Port][0], 10);

			if (UIDigi[Port])
			{
				free(UIDigi[Port]);
				UIDigi[Port] = NULL;
			}

			if (UIDigiAX[Port])
			{
				free(UIDigiAX[Port]);
				UIDigiAX[Port] = NULL;
			}

			GetDlgItemText(hDlg, IDC_UIDIGIS, Digis, 99); 
		
			UIDigi[Port] = _strdup(Digis);
		
			GetDlgItemText(hDlg, IDC_FILENAME, &FN[Port][0], 255); 
			GetDlgItemText(hDlg, IDC_MESSAGE, &Message[Port][0], 1000); 
	
			Interval[Port] = GetDlgItemInt(hDlg, IDC_INTERVAL, &OK, FALSE); 

			MinCounter[Port] = Interval[Port];

			SendFromFile[Port] = IsDlgButtonChecked(hDlg, IDC_FROMFILE);

			sprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\UIUtil\\UIPort%d", PortNum[CurrentPage]);

			retCode = RegCreateKeyEx(REGTREE,
					Key, 0, 0, 0, KEY_ALL_ACCESS, NULL, &hKey, &disp);
	
			if (retCode == ERROR_SUCCESS)
			{
				retCode = RegSetValueEx(hKey, "UIDEST", 0, REG_SZ,(BYTE *)&UIDEST[Port][0], strlen(&UIDEST[Port][0]));
				retCode = RegSetValueEx(hKey, "FileName", 0, REG_SZ,(BYTE *)&FN[Port][0], strlen(&FN[Port][0]));
				retCode = RegSetValueEx(hKey, "Message", 0, REG_SZ,(BYTE *)&Message[Port][0], strlen(&Message[Port][0]));
				retCode = RegSetValueEx(hKey, "Interval", 0, REG_DWORD,(BYTE *)&Interval[Port], 4);
				retCode = RegSetValueEx(hKey, "SendFromFile", 0, REG_DWORD,(BYTE *)&SendFromFile[Port], 4);
				retCode = RegSetValueEx(hKey, "Enabled", 0, REG_DWORD,(BYTE *)&UIEnabled[Port], 4);
				retCode = RegSetValueEx(hKey, "Digis",0, REG_SZ, Digis, strlen(Digis));

				RegCloseKey(hKey);
			}

			SetupUI(Port);

			return (INT_PTR)TRUE;


		case IDCANCEL:

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;

		case ID_TEST:

			SendBeacon(Port);
			return TRUE;




		}
		break;

	}	
	return (INT_PTR)FALSE;
}




VOID WINAPI OnTabbedDialogInit(HWND hDlg)
{
	DLGHDR *pHdr = (DLGHDR *) LocalAlloc(LPTR, sizeof(DLGHDR));
	DWORD dwDlgBase = GetDialogBaseUnits();
	int cxMargin = LOWORD(dwDlgBase) / 4;
	int cyMargin = HIWORD(dwDlgBase) / 8;

	TC_ITEM tie;
	RECT rcTab;

	int i, pos, tab = 0;
	INITCOMMONCONTROLSEX init;

	char PortNo[60];
	struct _EXTPORTDATA * PORTVEC;

	hwndDlg = hDlg;			// Save Window Handle

	// Save a pointer to the DLGHDR structure.

	SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) pHdr);

	// Create the tab control.


	init.dwICC = ICC_STANDARD_CLASSES;
	init.dwSize=sizeof(init);
	i=InitCommonControlsEx(&init);

	pHdr->hwndTab = CreateWindow(WC_TABCONTROL, "", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
		0, 0, 100, 100, hwndDlg, NULL, hInst, NULL);

	if (pHdr->hwndTab == NULL) {

	// handle error

	}

	// Add a tab for each of the child dialog boxes.

	tie.mask = TCIF_TEXT | TCIF_IMAGE;

	tie.iImage = -1;

	for (i = 1; i <= GetNumberofPorts(); i++)
	{
		// Only allow UI on ax.25 ports

		PORTVEC = (struct _EXTPORTDATA * )GetPortTableEntry(i);

		if (PORTVEC->PORTCONTROL.PORTTYPE == 16)		// EXTERNAL
			if (PORTVEC->PORTCONTROL.PROTOCOL == 10)	// Pactor/WINMOR
				continue;

		sprintf(PortNo, "Port %2d", GetPortNumber(i));
		PortNum[tab] = GetPortNumber(i);

		tie.pszText = PortNo;
		TabCtrl_InsertItem(pHdr->hwndTab, tab, &tie);
	
		pHdr->apRes[tab++] = DoLockDlgRes("PORTPAGE");

	}

	PageCount = tab;

	// Determine the bounding rectangle for all child dialog boxes.

	SetRectEmpty(&rcTab);

	for (i = 0; i < PageCount; i++)
	{
		if (pHdr->apRes[i]->cx > rcTab.right)
			rcTab.right = pHdr->apRes[i]->cx;

		if (pHdr->apRes[i]->cy > rcTab.bottom)
			rcTab.bottom = pHdr->apRes[i]->cy;

	}

	MapDialogRect(hwndDlg, &rcTab);

//	rcTab.right = rcTab.right * LOWORD(dwDlgBase) / 4;

//	rcTab.bottom = rcTab.bottom * HIWORD(dwDlgBase) / 8;

	// Calculate how large to make the tab control, so

	// the display area can accomodate all the child dialog boxes.

	TabCtrl_AdjustRect(pHdr->hwndTab, TRUE, &rcTab);

	OffsetRect(&rcTab, cxMargin - rcTab.left, cyMargin - rcTab.top);

	// Calculate the display rectangle.

	CopyRect(&pHdr->rcDisplay, &rcTab);

	TabCtrl_AdjustRect(pHdr->hwndTab, FALSE, &pHdr->rcDisplay);

	// Set the size and position of the tab control, buttons,

	// and dialog box.

	SetWindowPos(pHdr->hwndTab, NULL, rcTab.left, rcTab.top, rcTab.right - rcTab.left, rcTab.bottom - rcTab.top, SWP_NOZORDER);

	// Move the Buttons to bottom of page

	pos=rcTab.left+cxMargin;

	
	// Size the dialog box.

	SetWindowPos(hwndDlg, NULL, 0, 0, rcTab.right + cyMargin + 2 * GetSystemMetrics(SM_CXDLGFRAME),
		rcTab.bottom  + 2 * cyMargin + 2 * GetSystemMetrics(SM_CYDLGFRAME) + GetSystemMetrics(SM_CYCAPTION),
		SWP_NOMOVE | SWP_NOZORDER);

	// Simulate selection of the first item.

	OnSelChanged(hwndDlg);

}

// DoLockDlgRes - loads and locks a dialog template resource.

// Returns a pointer to the locked resource.

// lpszResName - name of the resource

DLGTEMPLATE * WINAPI DoLockDlgRes(LPCSTR lpszResName)
{
	HRSRC hrsrc = FindResource(NULL, lpszResName, RT_DIALOG);
	HGLOBAL hglb = LoadResource(hInst, hrsrc);

	return (DLGTEMPLATE *) LockResource(hglb);
}

//The following function processes the TCN_SELCHANGE notification message for the main dialog box. The function destroys the dialog box for the outgoing page, if any. Then it uses the CreateDialogIndirect function to create a modeless dialog box for the incoming page.

// OnSelChanged - processes the TCN_SELCHANGE notification.

// hwndDlg - handle of the parent dialog box

VOID WINAPI OnSelChanged(HWND hwndDlg)
{
	char PortDesc[40];
	int Port;

	DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndDlg, GWL_USERDATA);

	CurrentPage = TabCtrl_GetCurSel(pHdr->hwndTab);

	// Destroy the current child dialog box, if any.

	if (pHdr->hwndDisplay != NULL)

		DestroyWindow(pHdr->hwndDisplay);

	// Create the new child dialog box.

	pHdr->hwndDisplay = CreateDialogIndirect(hInst, pHdr->apRes[CurrentPage], hwndDlg, ChildDialogProc);

	hwndDisplay = pHdr->hwndDisplay;		// Save

	Port = PortNum[CurrentPage];
	// Fill in the controls

	GetPortDescription(PortNum[CurrentPage], PortDesc);

	SetDlgItemText(hwndDisplay, IDC_PORTNAME, PortDesc);

//	CheckDlgButton(hwndDisplay, IDC_FROMFILE, SendFromFile[Port]);

//	SetDlgItemInt(hwndDisplay, IDC_INTERVAL, Interval[Port], FALSE);

	SetDlgItemText(hwndDisplay, IDC_UIDEST, &UIDEST[Port][0]);
	SetDlgItemText(hwndDisplay, IDC_UIDIGIS, UIDigi[Port]);



//	SetDlgItemText(hwndDisplay, IDC_FILENAME, &FN[Port][0]);
//	SetDlgItemText(hwndDisplay, IDC_MESSAGE, &Message[Port][0]);

	ShowWindow(pHdr->hwndDisplay, SW_SHOWNORMAL);

}

//The following function processes the WM_INITDIALOG message for each of the child dialog boxes. You cannot specify the position of a dialog box created using the CreateDialogIndirect function. This function uses the SetWindowPos function to position the child dialog within the tab control's display area.

// OnChildDialogInit - Positions the child dialog box to fall

// within the display area of the tab control.

VOID WINAPI OnChildDialogInit(HWND hwndDlg)
{
	HWND hwndParent = GetParent(hwndDlg);
	DLGHDR *pHdr = (DLGHDR *) GetWindowLong(hwndParent, GWL_USERDATA);

	SetWindowPos(hwndDlg, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE);
}


*/


VOID ApplSendAPRSMessage(char * Text, char * ToCall)
{
	struct APRSMESSAGE * Message;
	struct APRSMESSAGE * ptr = OutstandingMsgs;
	int n = 0;
	char Msg[255];

	Message = malloc(sizeof(struct APRSMESSAGE));
	memset(Message, 0, sizeof(struct APRSMESSAGE));
	strcpy(Message->FromCall, APRSCall);
	memset(Message->ToCall, ' ', 9);
	memcpy(Message->ToCall, ToCall, strlen(ToCall));
	Message->ToStation = FindStation(ToCall, TRUE);

	if (Message->ToStation->LastRXSeq[0])		// Have we received a Reply-Ack message from him?
		sprintf(Message->Seq, "%02X}%c%c", NextSeq++, Message->ToStation->LastRXSeq[0], Message->ToStation->LastRXSeq[1]);
	else
	{
		if (Message->ToStation->SimpleNumericSeq)
			sprintf(Message->Seq, "%d", NextSeq++);
		else
			sprintf(Message->Seq, "%02X}", NextSeq++);	// Don't know, so assume message-ack capable
	}
	strcpy(Message->Text, Text);
	Message->Retries = RetryCount;
	Message->RetryTimer = RetryTimer;

	if (ptr == NULL)
	{
		OutstandingMsgs = Message;
	}
	else
	{
		n++;
		while(ptr->Next)
		{
			ptr = ptr->Next;
			n++;
		}
		ptr->Next = Message;
	}

//	UpdateTXMessageLine(n, Message);

	n = sprintf(Msg, ":%-9s:%s{%s", ToCall, Text, Message->Seq);

	PutAPRSMessage(Msg, n);
	return;
}


VOID ProcessMessage(char * Payload, struct STATIONRECORD * Station)
{
	char MsgDest[10];
	struct APRSMESSAGE * Message;
	struct APRSMESSAGE * ptr = Messages;
	char * TextPtr = &Payload[11];
	char * SeqPtr;
	int n = 0;
	char FromCall[10] = "         ";
	struct tm * TM;
	time_t NOW;

	memcpy(FromCall, Station->Callsign, strlen(Station->Callsign));
	memcpy(MsgDest, &Payload[1], 9);
	MsgDest[9] = 0;

	SeqPtr = strchr(TextPtr, '{');

	if (SeqPtr)
	{
		*(SeqPtr++) = 0;
		if(strlen(SeqPtr) > 6)
			SeqPtr[7] = 0;		
	}

	if (_memicmp(TextPtr, "ack", 3) == 0)
	{
		// Message Ack. See if for one of our messages

		ptr = OutstandingMsgs;

		if (ptr == 0)
			return;

		do
		{
			if (strcmp(ptr->FromCall, MsgDest) == 0
				&& strcmp(ptr->ToCall, FromCall) == 0
				&& strcmp(ptr->Seq, &TextPtr[3]) == 0)
			{
				// Message is acked

				ptr->Retries = 0;
				ptr->Acked = TRUE;
//				if (hMsgsOut)
//					UpdateTXMessageLine(hMsgsOut, n, ptr);

				return;
			}
			ptr = ptr->Next;
			n++;

		} while (ptr);
	
		return;
	}

	Message = malloc(sizeof(struct APRSMESSAGE));
	memset(Message, 0, sizeof(struct APRSMESSAGE));
	strcpy(Message->FromCall, Station->Callsign);
	strcpy(Message->ToCall, MsgDest);

	if (SeqPtr)
	{
		strcpy(Message->Seq, SeqPtr);

		// If a REPLY-ACK Seg, copy to LastRXSeq, and see if it acks a message

		if (SeqPtr[2] == '}')
		{
			struct APRSMESSAGE * ptr1;
			int nn = 0;

			strcpy(Station->LastRXSeq, SeqPtr);

			ptr1 = OutstandingMsgs;

			while (ptr1)
			{
				if (strcmp(ptr1->FromCall, MsgDest) == 0
					&& strcmp(ptr1->ToCall, FromCall) == 0
					&& memcmp(&ptr1->Seq, &SeqPtr[3], 2) == 0)
				{
					// Message is acked

					ptr1->Acked = TRUE;
					ptr1->Retries = 0;
//					if (hMsgsOut)
//						UpdateTXMessageLine(hMsgsOut, nn, ptr);
					
					break;
				}
				ptr1 = ptr1->Next;
				nn++;
			}
		}
		else
		{
			// Station is not using reply-ack - set to send simple numeric sequence (workround for bug in APRS Messanger
		
			Station->SimpleNumericSeq = TRUE;
		}
	}

	if (strlen(TextPtr) > 100)
		TextPtr[100] = 0;

	strcpy(Message->Text, TextPtr);
		
	NOW = time(NULL);

	if (LocalTime)
		TM = localtime(&NOW);
	else
		TM = gmtime(&NOW);
					
	sprintf(Message->Time, "%.2d:%.2d", TM->tm_hour, TM->tm_min);

	if (_stricmp(MsgDest, APRSCall) == 0 && SeqPtr)	// ack it if it has a sequence
	{
		// For us - send an Ack

		char ack[30];

		int n = sprintf(ack, ":%-9s:ack%s", Message->FromCall, Message->Seq);
		PutAPRSMessage(ack, n);
	}

	if (ptr == NULL)
	{
		Messages = Message;
	}
	else
	{
		n++;
		while(ptr->Next)
		{
			ptr = ptr->Next;
			n++;
		}
		ptr->Next = Message;
	}

	if (strcmp(MsgDest, APRSCall) == 0)			// to me?
	{
	}
}

VOID APRSSecTimer()
{

	// Check Message Retries

	struct APRSMESSAGE * ptr = OutstandingMsgs;
	int n = 0;

	if (SendWX)
		SendWeatherBeacon();


	if (ptr == 0)
		return;

	do
	{				
		if (ptr->Acked == FALSE)
		{
			if (ptr->Retries)
			{
				ptr->RetryTimer--;
				
				if (ptr->RetryTimer == 0)
				{
					ptr->Retries--;

					if (ptr->Retries)
					{
						// Send Again
						
						char Msg[255];
						int n = sprintf(Msg, ":%-9s:%s{%s", ptr->ToCall, ptr->Text, ptr->Seq);
						PutAPRSMessage(Msg, n);
						ptr->RetryTimer = RetryTimer;
					}
//					UpdateTXMessageLine(hMsgsOut, n, ptr);
				}
			}
		}

		ptr = ptr->Next;
		n++;

	} while (ptr);
}

double radians(double Degrees)
{
    return M_PI * Degrees / 180;
}
double degrees(double Radians)
{
	return Radians * 180 / M_PI;
}

double Distance(double laa, double loa)
{
	double lah, loh;

	GetAPRSLatLon(&lah, &loh);

/*

'Great Circle Calculations.

'dif = longitute home - longitute away


'      (this should be within -180 to +180 degrees)
'      (Hint: This number should be non-zero, programs should check for
'             this and make dif=0.0001 as a minimum)
'lah = latitude of home
'laa = latitude of away

'dis = ArcCOS(Sin(lah) * Sin(laa) + Cos(lah) * Cos(laa) * Cos(dif))
'distance = dis / 180 * pi * ERAD
'angle = ArcCOS((Sin(laa) - Sin(lah) * Cos(dis)) / (Cos(lah) * Sin(dis)))

'p1 = 3.1415926535: P2 = p1 / 180: Rem -- PI, Deg =>= Radians
*/

	loh = radians(loh); lah = radians(lah);
	loa = radians(loa); laa = radians(laa);

	loh = 60*degrees(acos(sin(lah) * sin(laa) + cos(lah) * cos(laa) * cos(loa-loh))) * 1.15077945;
	return loh;
}

double Bearing(double lat2, double lon2)
{
	double lat1, lon1;
	double dlat, dlon, TC1;

	GetAPRSLatLon(&lat1, &lon1);
 
	lat1 = radians(lat1);
	lat2 = radians(lat2);
	lon1 = radians(lon1);
	lon2 = radians(lon2);

	dlat = lat2 - lat1;
	dlon = lon2 - lon1;

	if (dlat == 0 || dlon == 0) return 0;
	
	TC1 = atan((sin(lon1 - lon2) * cos(lat2)) / (cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(lon1 - lon2)));
	TC1 = degrees(TC1);
		
	if (fabs(TC1) > 89.5) if (dlon > 0) return 90; else return 270;

	if (dlat > 0)
	{
		if (dlon > 0) return -TC1;
		if (dlon < 0) return 360 - TC1;
		return 0;
	}

	if (dlat < 0)
	{
		if (dlon > 0) return TC1 = 180 - TC1;
		if (dlon < 0) return TC1 = 180 - TC1; // 'ok?
		return 180;
	}

	return 0;
}

// Weather Data 
	
static char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

VOID SendWeatherBeacon()
{
	char Msg[256];
	char DD[3]="";
	char HH[3]="";
	char MM[3]="";
	char Lat[10], Lon[10];
	int Len, index;
	char WXMessage[1024];
	char * WXptr;
	char * WXend;
	time_t WXTime;
	time_t now = time(NULL);
	FILE * hFile;
	struct tm * TM;
	struct stat STAT;
 
	WXCounter++;

	if (WXCounter < WXInterval * 60)
		return;

	WXCounter = 0;

//	Debugprintf("BPQAPRS - Trying to open WX file %s", WXFileName);

	if (stat(WXFileName, &STAT))
	{
		Debugprintf("APRS WX File %s stat() falied %d", WXFileName, GetLastError());
		return;
	}

	WXTime = (now - STAT.st_mtime) /60;			// Minutes

	if (WXTime > (3 * WXInterval))
	{
		Debugprintf("APRS Send WX File %s too old - %d minutes", WXFileName, WXTime);
		return;
	}
	
	hFile = fopen(WXFileName, "rb");
	
	if (hFile)
		Len = fread(WXMessage, 1, 1024, hFile); 
	else
	{
		Debugprintf("APRS WX File %s open() failed %d", WXFileName, GetLastError());
		return;
	}

	
	if (Len < 30)
	{
		Debugprintf("BPQAPRS - WX file %s is too short - %d Chars", WXFileName, Len);
		fclose(hFile);
		return;
	}

	// see if wview format

//04-09-13, 2245
//TempIn 23
//TempEx 18
//WindHi 0
//WindAv 0
//WindDr 200
//BarmPs 30167
//HumdIn 56
//HumdEx 100
//RnFall 0.00
//DailyRnFall 0.00

	if (strstr(WXMessage, "TempIn"))
	{
		int Wind  =  0;
		int Gust = 0;
		int Temp = 0;
		int Winddir = 0;
		int Humidity = 0;
		int Raintoday = 0;
		int Rain24hrs = 0;
		int Pressure = 0;

		char * ptr;

		ptr = strstr(WXMessage, "TempEx");
		if (ptr)
			Temp = (atof(ptr + 7) * 1.8) + 32;

		ptr = strstr(WXMessage, "WindHi");
		if (ptr)
			Gust = atoi(ptr + 7);

		ptr = strstr(WXMessage, "WindAv");
		if (ptr)
			Wind = atoi(ptr + 7);

		ptr = strstr(WXMessage, "WindDr");
		if (ptr)
			Winddir = atoi(ptr + 7);

		ptr = strstr(WXMessage, "BarmPs");
		if (ptr)
			Pressure = atof(ptr + 7) * 0.338638866667;			// Inches to 1/10 millbars

		ptr = strstr(WXMessage, "HumdEx");
		if (ptr)
			Humidity = atoi(ptr + 7);

		ptr = strstr(WXMessage, "RnFall");
		if (ptr)
			Rain24hrs = atof(ptr + 7) * 100.0;

		ptr = strstr(WXMessage, "DailyRnFall");
		if (ptr)
			Raintoday = atof(ptr + 12) * 100.0;

		if (Humidity > 99)
			Humidity = 99;
		
		sprintf(WXMessage, "%03d/%03dg%03dt%03dr%03dP%03dp%03dh%02db%05d",
			Winddir, Wind, Gust, Temp, 0, Raintoday, Rain24hrs, Humidity, Pressure);

	}

	WXptr = strchr(WXMessage, 10);

	if (WXptr)
	{
		WXend = strchr(++WXptr, 13);
		if (WXend == 0)
			WXend = strchr(WXptr, 10);
		if (WXend)
			*WXend = 0;
	}
	else
		WXptr = &WXMessage[0];

	// Get DDHHMM from Filetime

	TM = gmtime(&STAT.st_mtime);

	sprintf(DD, "%02d", TM->tm_mday);
	sprintf(HH, "%02d", TM->tm_hour);
	sprintf(MM, "%02d", TM->tm_min);

	GetAPRSLatLonString(Lat, Lon);

	Len = sprintf(Msg, "@%s%s%sz%s/%s_%s%s", DD, HH, MM, Lat, Lon, WXptr, WXComment);

	Debugprintf(Msg);

	for (index = 0; index < 32; index++)
		if (WXPort[index])
			SendAPRSMessage(Msg, index);

	fclose(hFile);
}


/*
Jan 22 2012 14:10
123/005g011t031r000P000p000h00b10161

/MITWXN Mitchell IN weather Station N9LYA-3 {UIV32} 
< previous

@221452z3844.42N/08628.33W_203/006g007t032r000P000p000h00b10171
Complete Weather Report Format — with Lat/Long position, no Timestamp
! or = Lat   Sym Table ID   Long   Symbol Code _  Wind Directn/ Speed Weather Data APRS Software   WX Unit uuuu
 1      8          1         9          1                 7                 n            1              2-4
Examples
!4903.50N/07201.75W_220/004g005t077r000p000P000h50b09900wRSW
!4903.50N/07201.75W_220/004g005t077r000p000P000h50b.....wRSW

*/

//	Web Server Code

//	The actual HTTP socket code is in bpq32.dll. Any requests for APRS data are passed in 
//	using a Named Pipe. The request looks exactly like one from a local socket, and the respone is
//	a fully pormatted HTTP packet


#define InputBufferLen 1000


#define MaxSessions 100


HANDLE PipeHandle;

int HTTPPort = 80;
BOOL IPV6 = TRUE;

#define MAX_PENDING_CONNECTS 5

BOOL OpenSockets6();

char HTDocs[MAX_PATH] = "HTML";
char SpecialDocs[MAX_PATH] = "Special Pages";

char SymbolText[192][20] = {

"Police Stn", "No Symbol", "Digi", "Phone", "DX Cluster", "HF Gateway", "Plane sm", "Mob Sat Stn",
"WheelChair", "Snowmobile", "Red Cross", "Boy Scout", "Home", "X", "Red Dot", "Circle (0)", 
"Circle (1)", "Circle (2)", "Circle (3)", "Circle (4)", "Circle (5)", "Circle (6)", "Circle (7)", "Circle (8)", 
"Circle (9)", "Fire", "Campground", "Motorcycle", "Rail Eng.", "Car", "File svr", "HC Future", 

"Aid Stn", "BBS", "Canoe", "No Symbol", "Eyeball", "Tractor", "Grid Squ.", "Hotel", 
"Tcp/ip", "No Symbol", "School", "Usr Log-ON", "MacAPRS", "NTS Stn", "Balloon", "Police", 
"TBD", "Rec Veh'le", "Shuttle", "SSTV", "Bus", "ATV", "WX Service", "Helo", 
"Yacht", "WinAPRS", "Jogger", "Triangle", "PBBS", "Plane lrge", "WX Station", "Dish Ant.", 

"Ambulance", "Bike", "ICP", "Fire Station", "Horse", "Fire Truck", "Glider", "Hospital", 
"IOTA", "Jeep", "Truck", "Laptop", "Mic-E Rptr", "Node", "EOC", "Rover", 
"Grid squ.", "Antenna", "Power Boat", "Truck Stop", "Truck 18wh", "Van", "Water Stn", "XAPRS", 
"Yagi", "Shelter", "No Symbol", "No Symbol", "No Symbol", "No Symbol", "", "",

"Emergency", "No Symbol", "No. Digi", "Bank", "No Symbol", "No. Diam'd", "Crash site", "Cloudy", 
"MEO", "Snow", "Church", "Girl Scout", "Home (HF)", "UnknownPos", "Destination", "No. Circle", 
"No Symbol", "No Symbol", "No Symbol", "No Symbol", "No Symbol", "No Symbol", "No Symbol", "No Symbol", 
"Petrol Stn", "Hail", "Park", "Gale Fl", "No Symbol", "No. Car", "Info Kiosk", "Hurricane", 

"No. Box", "Snow blwng", "Coast G'rd", "Drizzle", "Smoke", "Fr'ze Rain", "Snow Shwr", "Haze", 
"Rain Shwr", "Lightning", "Kenwood", "Lighthouse", "No Symbol", "Nav Buoy", "Rocket", "Parking  ", 
"Quake", "Restaurant", "Sat/Pacsat", "T'storm", "Sunny", "VORTAC", "No. WXS", "Pharmacy", 
"No Symbol", "No Symbol", "Wall Cloud", "No Symbol", "No Symbol", "No. Plane", "No. WX Stn", "Rain",

"No. Diamond", "Dust blwng", "No. CivDef", "DX Spot", "Sleet", "Funnel Cld", "Gale", "HAM store",
"No. Blk Box", "WorkZone", "SUV", "Area Locns", "Milepost", "No. Triang", "Circle sm", "Part Cloud",
"No Symbol", "Restrooms", "No. Boat", "Tornado", "No. Truck", "No. Van", "Flooding", "No Symbol",
"Sky Warn", "No Symbol", "Fog", "No Symbol", "No Symbol", "No Symbol", "", ""};

// All Calls (8 per line)

//<td><a href="find.cgi?call=EI7IG-1">EI7IG-1</a></td>
//<td><a href="find.cgi?call=G7TKK-1">G7TKK-1</a></td>
//<td><a href="find.cgi?call=GB7GL-B">GB7GL-B</a></td>
//<td><a href="find.cgi?call=GM1TCN">GM1TCN</a></td>
//<td><a href="find.cgi?call=GM8BPQ">GM8BPQ</a></td>
//<td><a href="find.cgi?call=GM8BPQ-14">GM8BPQ-14</a></td>
//<td><a href="find.cgi?call=LA2VPA-9">LA2VPA-9</a></td>
//<td><a href="find.cgi?call=LA3FIA-10">LA3FIA-10</a></td></tr><tr>
//<td><a href="find.cgi?call=LA6JF-2">LA6JF-2</a></td><td><a href="find.cgi?call=LD4ST">LD4ST</a></td><td><a href="find.cgi?call=M0CHK-7">M0CHK-7</a></td><td><a href="find.cgi?call=M0OZH-7">M0OZH-7</a></td><td><a href="find.cgi?call=MB7UFO-1">MB7UFO-1</a></td><td><a href="find.cgi?call=MB7UN">MB7UN</a></td><td><a href="find.cgi?call=MM0DXE-15">MM0DXE-15</a></td><td><a href="find.cgi?call=PA2AYX-9">PA2AYX-9</a></td></tr><tr>
//<td><a href="find.cgi?call=PA3AQW-5">PA3AQW-5</a></td><td><a href="find.cgi?call=PD1C">PD1C</a></td><td><a href="find.cgi?call=PD5LWD-2">PD5LWD-2</a></td><td><a href="find.cgi?call=PI1ECO">PI1ECO</a></td></tr>


char * DoSummaryLine(struct STATIONRECORD * ptr, int n, int Width)
{
	static char Line2[80];
	int x;
	char XCall[256];
	char * ptr1 = ptr->Callsign;
	char * ptr2 = XCall;

	// Object Names can contain spaces

	while(*ptr1)
	{
		if (*ptr1 == ' ')
		{		
			memcpy(ptr2, "%20", 3);
			ptr2 += 3;
		}
		else
			*(ptr2++) = *ptr1;

		ptr1++;
	}

	*ptr2 = 0;


	// Object Names can contain spaces
	

	sprintf(Line2, "<td><a href=""find.cgi?call=%s"">%s</a></td>",
		XCall, ptr->Callsign);

	x = ++n/Width;
	x = x * Width;

	if (x == n)
		strcat(Line2, "</tr><tr>");

	return Line2;
}

char * DoDetailLine(struct STATIONRECORD * ptr)
{
	static char Line[512];
	double Lat = ptr->Lat;
	double Lon = ptr->Lon;
	char NS='N', EW='E';

	char LatString[20], LongString[20], DistString[20], BearingString[20];
	int Degrees;
	double Minutes;
	char Time[80];
	struct tm * TM;
	char XCall[256];

	char * ptr1 = ptr->Callsign;
	char * ptr2 = XCall;

	// Object Names can contain spaces

	while(*ptr1)
	{
		if (*ptr1 == ' ')
		{		
			memcpy(ptr2, "%20", 3);
			ptr2 += 3;
		}
		else
			*(ptr2++) = *ptr1;

		ptr1++;
	}

	*ptr2 = 0;

	
//	if (ptr->ObjState == '_')	// Killed Object
//		return;

	TM = gmtime(&ptr->TimeLastUpdated);

	sprintf(Time, "%.2d:%.2d:%.2d", TM->tm_hour, TM->tm_min, TM->tm_sec);

	if (ptr->Lat < 0)
	{
		NS = 'S';
		Lat=-Lat;
	}
	if (Lon < 0)
	{
		EW = 'W';
		Lon=-Lon;
	}

#pragma warning(push)
#pragma warning(disable:4244)

	Degrees = Lat;
	Minutes = Lat * 60.0 - (60 * Degrees);

	sprintf(LatString,"%2d°%05.2f'%c", Degrees, Minutes, NS);
		
	Degrees = Lon;

#pragma warning(pop)

	Minutes = Lon * 60 - 60 * Degrees;

	sprintf(LongString, "%3d°%05.2f'%c",Degrees, Minutes, EW);

	sprintf(DistString, "%6.1f", Distance(ptr->Lat, ptr->Lon));
	sprintf(BearingString, "%3.0f", Bearing(ptr->Lat, ptr->Lon));
	
	sprintf(Line, "<tr><td align=""left""><a href=""find.cgi?call=%s"">&nbsp;%s%s</a></td><td align=""left"">%s</td><td align=""center"">%s  %s</td><td align=""right"">%s</td><td align=""right"">%s</td><td align=""left"">%s</td></tr>",
			XCall, ptr->Callsign, 
			(strchr(ptr->Path, '*'))?  "*": "", &SymbolText[ptr->iconRow << 4 | ptr->iconCol][0], LatString, LongString, DistString, BearingString, Time);

	return Line;
}

 
int CompareFN(const void *a, const void *b) 
{
	const struct STATIONRECORD * x = a;
	const struct STATIONRECORD * y = b;

	x = x->Next;
	y = y->Next;

	return strcmp(x->Callsign, y->Callsign);

	/* strcmp functions works exactly as expected from
	comparison function */ 
} 



char * CreateStationList(BOOL RFOnly, BOOL WX, BOOL Mobile, char Objects, int * Count, char * Param)
{
	char * Line = malloc(100000);
	struct STATIONRECORD * ptr = *StationRecords;
	int n = 0, i;
	struct STATIONRECORD * List[1000];
	int TableWidth = 8;

	Line[0] = 0;
	
	if (Param && Param[0])
	{
		char * Key, *Context;

		Key = strtok_s(Param, "=", &Context);

		TableWidth = atoi(Context);

		if (TableWidth == 0)
			TableWidth = 8;
	}

	// Build list of calls

	while (ptr)
	{
		if (ptr->ObjState == Objects && ptr->Lat != 0.0 && ptr->Lon != 0.0)
		{
			if ((WX && (ptr->LastWXPacket[0] == 0)) || (RFOnly && (ptr->LastPort == 0)) ||
				(Mobile && ((ptr->Speed < 0.1) || ptr->LastWXPacket[0] != 0)))
			{
				ptr = ptr->Next;
				continue;
			}

			List[n++] = ptr;

			if (n > 999)
				break;

		}
		ptr = ptr->Next;		
	}

	if (n >  1)
		qsort(List, n, 4, CompareFN);

	for (i = 0; i < n; i++)
	{
		if (RFOnly)
			strcat(Line, DoDetailLine(List[i]));
		else
			strcat(Line, DoSummaryLine(List[i], i, TableWidth));
	}	
		
	*Count = n;

	return Line;

}

char * APRSLookupKey(struct APRSConnectionInfo * sockptr, char * Key)
{
	struct STATIONRECORD * stn = sockptr->SelCall;

	if (strcmp(Key, "##MY_CALLSIGN##") == 0)
		return _strdup(LoppedAPRSCall);

	if (strcmp(Key, "##CALLSIGN##") == 0)
		return _strdup(sockptr->Callsign);

	if (strcmp(Key, "##CALLSIGN_NOSSID##") == 0)
	{
		char * Call = _strdup(sockptr->Callsign);
		char * ptr = strchr(Call, '-');
		if (ptr)
			*ptr = 0;
		return Call;
	}

	if (strcmp(Key, "##MY_WX_CALLSIGN##") == 0)
		return _strdup(LoppedAPRSCall);

	if (strcmp(Key, "##MY_BEACON_COMMENT##") == 0)
		return _strdup(StatusMsg);

	if (strcmp(Key, "##MY_WX_BEACON_COMMENT##") == 0)
		return _strdup(WXComment);

	if (strcmp(Key, "##MILES_KM##") == 0)
		return _strdup("Miles");

	if (strcmp(Key, "##EXPIRE_TIME##") == 0)
	{
		char val[80];
		sprintf(val, "%d", ExpireTime);
		return _strdup(val);
	}

	if (strcmp(Key, "##LOCATION##") == 0)
	{
		char val[80];
		double Lat = sockptr->SelCall->Lat;
		double Lon = sockptr->SelCall->Lon;
		char NS='N', EW='E';
		char LatString[20];
		int Degrees;
		double Minutes;
	
		if (Lat < 0)
		{
			NS = 'S';
			Lat=-Lat;
		}
		if (Lon < 0)
		{
			EW = 'W';
			Lon=-Lon;
		}

#pragma warning(push)
#pragma warning(disable:4244)

		Degrees = Lat;
		Minutes = Lat * 60.0 - (60 * Degrees);

		sprintf(LatString,"%2d°%05.2f'%c",Degrees, Minutes, NS);
		
		Degrees = Lon;

#pragma warning(pop)

		Minutes = Lon * 60 - 60 * Degrees;

		sprintf(val,"%s %3d°%05.2f'%c", LatString, Degrees, Minutes, EW);

		return _strdup(val);
	}

	if (strcmp(Key, "##LOCDDMMSS##") == 0)
	{
		char val[80];
		double Lat = sockptr->SelCall->Lat;
		double Lon = sockptr->SelCall->Lon;
		char NS='N', EW='E';
		char LatString[20];
		int Degrees;
		double Minutes;

		// 48.45.18N, 002.18.37E
			
		if (Lat < 0)
		{
			NS = 'S';
			Lat=-Lat;
		}
		if (Lon < 0)
		{
			EW = 'W';
			Lon=-Lon;
		}

#pragma warning(push)
#pragma warning(disable:4244)

		Degrees = Lat;
		Minutes = Lat * 60.0 - (60 * Degrees);
//		IntMins = Minutes;
//		Seconds = Minutes * 60.0 - (60 * IntMins);

		sprintf(LatString,"%2d.%05.2f%c",Degrees, Minutes, NS);
		
		Degrees = Lon;
		Minutes = Lon * 60.0 - 60 * Degrees;
//		IntMins = Minutes;
//		Seconds = Minutes * 60.0 - (60 * IntMins);

#pragma warning(pop)

		sprintf(val,"%s, %03d.%05.2f%c", LatString, Degrees, Minutes, EW);

		return _strdup(val);
	}
	if (strcmp(Key, "##STATUS_TEXT##") == 0)
		return _strdup(stn->Status);
	
	if (strcmp(Key, "##LASTPACKET##") == 0)
		return _strdup(stn->LastPacket);


	if (strcmp(Key, "##LAST_HEARD##") == 0)
	{
		char Time[80];
		struct tm * TM;
		time_t Age = time(NULL) - stn->TimeLastUpdated;

		TM = gmtime(&Age);

		sprintf(Time, "%.2d:%.2d:%.2d", TM->tm_hour, TM->tm_min, TM->tm_sec);

		return _strdup(Time);
	}

	if (strcmp(Key, "##FRAME_HEADER##") == 0)
		return _strdup(stn->Path);

	if (strcmp(Key, "##FRAME_INFO##") == 0)
		return _strdup(stn->LastWXPacket);
	
	if (strcmp(Key, "##BEARING##") == 0)
	{
		char val[80];

		sprintf(val, "%03.0f", Bearing(sockptr->SelCall->Lat, sockptr->SelCall->Lon));
		return _strdup(val);
	}

	if (strcmp(Key, "##COURSE##") == 0)
	{
		char val[80];

		sprintf(val, "%03.0f", stn->Course);
		return _strdup(val);
	}

	if (strcmp(Key, "##SPEED_MPH##") == 0)
	{
		char val[80];

		sprintf(val, "%5.1f", stn->Speed);
		return _strdup(val);
	}

	if (strcmp(Key, "##DISTANCE##") == 0)
	{
		char val[80];

		sprintf(val, "%5.1f", Distance(sockptr->SelCall->Lat, sockptr->SelCall->Lon));
		return _strdup(val);
	}



	if (strcmp(Key, "##WIND_DIRECTION##") == 0)
	{
		char val[80];

		sprintf(val, "%03d", sockptr->WindDirn);
		return _strdup(val);
	}

	if (strcmp(Key, "##WIND_SPEED_MPH##") == 0)
	{
		char val[80];

		sprintf(val, "%d", sockptr->WindSpeed);
		return _strdup(val);
	}

	if (strcmp(Key, "##WIND_GUST_MPH##") == 0)
	{
		char val[80];

		sprintf(val, "%d", sockptr->WindGust);
		return _strdup(val);
	}

	if (strcmp(Key, "##TEMPERATURE_F##") == 0)
	{
		char val[80];

		sprintf(val, "%d", sockptr->Temp);
		return _strdup(val);
	}

	if (strcmp(Key, "##HUMIDITY##") == 0)
	{
		char val[80];

		sprintf(val, "%d", sockptr->Humidity);
		return _strdup(val);
	}

	if (strcmp(Key, "##PRESSURE_HPA##") == 0)
	{
		char val[80];

		sprintf(val, "%05.1f", sockptr->Pressure /10.0);
		return _strdup(val);
	}

	if (strcmp(Key, "##RAIN_TODAY_IN##") == 0)
	{
		char val[80];

		sprintf(val, "%5.2f", sockptr->RainToday /100.0);
		return _strdup(val);
	}


	if (strcmp(Key, "##RAIN_24_IN##") == 0)
	{
		char val[80];

		sprintf(val, "%5.2f", sockptr->RainLastDay /100.0);
		return _strdup(val);
	}


	if (strcmp(Key, "##RAIN_HOUR_IN##") == 0)
	{
		char val[80];

		sprintf(val, "%5.2f", sockptr->RainLastHour /100.0);
		return _strdup(val);
	}

	if (strcmp(Key, "##MAP_LAT_LON##") == 0)
	{
		char val[256];

		sprintf(val, "%f,%f", stn->Lat, stn->Lon);
		return _strdup(val);
	}

	if (strcmp(Key, "##SYMBOL_DESCRIPTION##") == 0)
		return _strdup(&SymbolText[stn->iconRow << 4 | stn->iconCol][0]);


/*
##WIND_SPEED_MS## - wind speed metres/sec
##WIND_SPEED_KMH## - wind speed km/hour
##WIND_GUST_MPH## - wind gust miles/hour
##WIND_GUST_MS## - wind gust metres/sec
##WIND_GUST_KMH## - wind gust km/hour
##WIND_CHILL_F## - wind chill F
##WIND_CHILL_C## - wind chill C
##TEMPERATURE_C## - temperature C
##DEWPOINT_F## - dew point temperature F
##DEWPOINT_C## - dew point temperature C
##PRESSURE_IN## - pressure inches of mercury
##PRESSURE_HPA## - pressure hPa (mb)
##RAIN_HOUR_MM## - rain in last hour mm
##RAIN_TODAY_MM## - rain today mm
##RAIN_24_MM## - rain in last 24 hours mm
##FRAME_HEADER## - frame header of the last posit heard from the station
##FRAME_INFO## - information field of the last posit heard from the station
##MAP_LARGE_SCALE##" - URL of a suitable large scale map on www.vicinity.com
##MEDIUM_LARGE_SCALE##" - URL of a suitable medium scale map on www.vicinity.com
##MAP_SMALL_SCALE##" - URL of a suitable small scale map on www.vicinity.com
##MY_LOCATION## - 'Latitude', 'Longitude' in 'Station Setup'
##MY_STATUS_TEXT## - status text configured in 'Status Text'
##MY_SYMBOL_DESCRIPTION## - 'Symbol' that would be shown for our station in 'Station List'
##HIT_COUNTER## - The number of times the page has been accessed
##DOCUMENT_LAST_CHANGED## - The date/time the page was last edited

##FRAME_HEADER## - frame header of the last posit heard from the station
##FRAME_INFO## - information field of the last posit heard from the station

*/
	return NULL;
}

VOID APRSProcessSpecialPage(struct APRSConnectionInfo * sockptr, char * Buffer, int FileSize, char * StationTable, int Count, BOOL WX)
{
	// replaces ##xxx### constructs with the requested data

	char * NewMessage = malloc(250000);
	char * ptr1 = Buffer, * ptr2, * ptr3, * ptr4, * NewPtr = NewMessage;
	int PrevLen;
	int BytesLeft = FileSize;
	int NewFileSize = FileSize;
	char * StripPtr = ptr1;
	int HeaderLen;
	char Header[256];

	if (WX && sockptr->SelCall && sockptr->SelCall->LastWXPacket)
	{
		DecodeWXReport(sockptr, sockptr->SelCall->LastWXPacket);
	}

	// strip comments blocks

	while (ptr4 = strstr(ptr1, "<!--"))
	{
		ptr2 = strstr(ptr4, "-->");
		if (ptr2)
		{
			PrevLen = (ptr4 - ptr1);
			memcpy(StripPtr, ptr1, PrevLen);
			StripPtr += PrevLen;
			ptr1 = ptr2 + 3;
			BytesLeft = FileSize - (ptr1 - Buffer);
		}
	}


	memcpy(StripPtr, ptr1, BytesLeft);
	StripPtr += BytesLeft;

	BytesLeft = StripPtr - Buffer;

	FileSize = BytesLeft;
	NewFileSize = FileSize;
	ptr1 = Buffer;
	ptr1[FileSize] = 0;

loop:
	ptr2 = strstr(ptr1, "##");

	if (ptr2)
	{
		PrevLen = (ptr2 - ptr1);			// Bytes before special text
		
		ptr3 = strstr(ptr2+2, "##");

		if (ptr3)
		{
			char Key[80] = "";
			int KeyLen;
			char * NewText;
			int NewTextLen;

			ptr3 += 2;
			KeyLen = ptr3 - ptr2;

			if (KeyLen < 80)
				memcpy(Key, ptr2, KeyLen);

			if (strcmp(Key, "##STATION_TABLE##") == 0)
			{
				NewText = _strdup(StationTable);
			}
			else
			{
				if (strcmp(Key, "##TABLE_COUNT##") == 0)
				{
					char val[80];
					sprintf(val, "%d", Count);
					NewText = _strdup(val);
				}
				else
					NewText = APRSLookupKey(sockptr, Key);
			}
			
			if (NewText)
			{
				NewTextLen = strlen(NewText);
				NewFileSize = NewFileSize + NewTextLen - KeyLen;					
			//	NewMessage = realloc(NewMessage, NewFileSize);

				memcpy(NewPtr, ptr1, PrevLen);
				NewPtr += PrevLen;
				memcpy(NewPtr, NewText, NewTextLen);
				NewPtr += NewTextLen;

				free(NewText);
				NewText = NULL;
			}
			else
			{
				// Key not found, so just leave

				memcpy(NewPtr, ptr1, PrevLen + KeyLen);
				NewPtr += (PrevLen + KeyLen);
			}

			ptr1 = ptr3;			// Continue scan from here
			BytesLeft = Buffer + FileSize - ptr3;
		}
		else		// Unmatched ##
		{
			memcpy(NewPtr, ptr1, PrevLen + 2);
			NewPtr += (PrevLen + 2);
			ptr1 = ptr2 + 2;
		}
		goto loop;
	}

	// Copy Rest

	memcpy(NewPtr, ptr1, BytesLeft);
	
	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", NewFileSize);
	send(sockptr->sock, Header, HeaderLen, 0); 
	send(sockptr->sock, NewMessage, NewFileSize, 0); 

	free (NewMessage);
	free(StationTable);
	
	return;
}

VOID APRSSendMessageFile(struct APRSConnectionInfo * sockptr, char * FN)
{
	int FileSize = 0;
	char * MsgBytes;
	char * SaveMsgBytes;

	char MsgFile[MAX_PATH];
	FILE * hFile;
	BOOL Special = FALSE;
	int HeaderLen;
	char Header[256];
	char * Param;
	struct stat STAT;
	int Sent;


	FN = strtok_s(FN, "?", &Param);

	if (strcmp(FN, "/") == 0)
		sprintf_s(MsgFile, sizeof(MsgFile), "%s/%s/%s/index.html", BPQDirectory, APRSDir, SpecialDocs);
	else
		sprintf_s(MsgFile, sizeof(MsgFile), "%s/%s/%s%s", BPQDirectory, APRSDir, SpecialDocs, &FN[5]);
	
	hFile = fopen(MsgFile, "rb");

	if (hFile == NULL)
	{
		// Try normal pages

		if (strcmp(FN, "/") == 0)
			sprintf_s(MsgFile, sizeof(MsgFile), "%s/%s/%s/index.html", BPQDirectory, APRSDir, HTDocs);
		else
			sprintf_s(MsgFile, sizeof(MsgFile), "%s/%s/%s%s", BPQDirectory,APRSDir, HTDocs, &FN[5]);
	
		hFile = fopen(MsgFile, "rb");

		if (hFile == NULL)
		{
			HeaderLen = sprintf(Header, "HTTP/1.1 404 Not Found\r\nContent-Length: 16\r\n\r\nPage not found\r\n");
			send(sockptr->sock, Header, HeaderLen, 0); 
			return;

		}
	}
	else
		Special = TRUE;

	if (stat(MsgFile, &STAT) == 0)
		FileSize = STAT.st_size;

	MsgBytes = SaveMsgBytes = malloc(FileSize+1);

	fread(MsgBytes, 1, FileSize, hFile); 

	fclose(hFile);

	// if HTML file, look for ##...## substitutions

	if ((strstr(FN, "htm" ) || strstr(FN, "HTM")) &&  strstr(MsgBytes, "##" ))
	{
		// Build Station list, depending on URL
	
		int Count = 0;
		BOOL RFOnly = (BOOL)strstr(_strlwr(FN), "rf");		// Leaves FN in lower case
		BOOL WX = (BOOL)strstr(FN, "wx");
		BOOL Mobile = (BOOL)strstr(FN, "mobile");
		char Objects = (strstr(FN, "obj"))? '*' :0;
		char * StationList;
				
		StationList = CreateStationList(RFOnly, WX, Mobile, Objects, &Count, Param);

		APRSProcessSpecialPage(sockptr, MsgBytes, FileSize, StationList, Count, WX); 
		free (MsgBytes);
		return;			// ProcessSpecial has sent the reply
	}

	HeaderLen = sprintf(Header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", FileSize);
	send(sockptr->sock, Header, HeaderLen, 0); 
	
	Sent = send(sockptr->sock, MsgBytes, FileSize, 0);
//	printf("Send %d %d\n", FileSize, Sent); 

	while (Sent < FileSize)
	{
		FileSize -= Sent;
		MsgBytes += Sent;
		Sent = send(sockptr->sock, MsgBytes, FileSize, 0);
//		printf("Send %d %d\n", FileSize, Sent); 
		if (Sent == -1)
		{
			Sleep(10);
			Sent = 0;
		}
	}

	free (SaveMsgBytes);
}


char PipeFileName[] = "\\\\.\\pipe\\BPQAPRSWebPipe";

VOID APRSProcessHTTPMessage(SOCKET sock, char * MsgPtr)
{
	int InputLen = 0;
	int OutputLen = 0;
   	char * URL;
	char * ptr;
	struct APRSConnectionInfo CI;
	struct APRSConnectionInfo * sockptr = &CI;
	char Key[12] = "";

	memset(&CI, 0, sizeof(CI));

	sockptr->sock = sock;

	if (memcmp(MsgPtr, "GET" , 3) != 0)
	{
		Debugprintf(MsgPtr);
		return;
	}

	URL = &MsgPtr[4];

	ptr = strstr(URL, " HTTP");

	if (ptr)
		*ptr = 0;

	if (_memicmp(URL, "/aprs/find.cgi?call=", 20) == 0)
	{
		// return Station details

		char * Call = &URL[20];
		BOOL RFOnly, WX, Mobile, Object = FALSE;
		struct STATIONRECORD * stn;
		char * Referrer = strstr(ptr + 1, "Referer:");

		// Undo any % transparency in call

		char * ptr1 = Call;
		char * ptr2 = Key;
		char c;

		c = *(ptr1++);

		while (c)
		{
			if (c == '%')
			{
				int n;
				int m = *(ptr1++) - '0';
				if (m > 9) m = m - 7;
				n = *(ptr1++) - '0';
				if (n > 9) n = n - 7;

				*(ptr2++) = m * 16 + n;
			}
			else if (c == '+')
				*(ptr2++) = ' ';
			else
				*(ptr2++) = c;

			c = *(ptr1++);
		}

		*(ptr2++) = 0;

		if (Referrer)
		{
			ptr = strchr(Referrer, 13);
			if (ptr)
			{
				*ptr = 0;
				RFOnly = (BOOL)strstr(Referrer, "rf");
				WX = (BOOL)strstr(Referrer, "wx");
				Mobile = (BOOL)strstr(Referrer, "mobile");
				Object = (BOOL)strstr(Referrer, "obj");

				if (WX)
					strcpy(URL, "/aprs/infowx_call.html");
				else if (Mobile)
					strcpy(URL, "/aprs/infomobile_call.html");
				else if (Object)
					strcpy(URL, "/aprs/infoobj_call.html");
				else
					strcpy(URL, "/aprs/info_call.html");
			}
		}

		if (Object)
		{
			// Name is space padded, and could have embedded spaces
				
			int Keylen = strlen(Key);
				
			if (Keylen < 9)
				memset(&Key[Keylen], 32, 9 - Keylen);
		}			
			
		stn = FindStation(Key, FALSE);

		if (stn == NULL)
			strcpy(URL, "/aprs/noinfo.html");
		else
			sockptr->SelCall = stn;
	}

	strcpy(sockptr->Callsign, Key);

	APRSSendMessageFile(sockptr, URL);

	return;
}
