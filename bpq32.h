/*

Declarations of BPQ32 API funtions

There are two sets of definitions, one for static linking against bpq32.lib, and one for
dynamic linking using LoadLibrary/GetProcAddress

Define symbol DYNLOADBPQ before including this file to use the dynamic form.

If you are writing an External Driver, rather than an application,
you must use Dynamic Linking, and you must also define symbol EXTDLL, which will
make the code use GetProcAddress rather than LoadLibrary. Without this the reference 
count on BPQ32.dll gets messed up, and the code will not unload cleanly.

*/


#ifndef DYNLOADBPQ

// Definitions for Statically Linked DLL

struct PORTCONTROL * APIENTRY GetPortTableEntryFromPortNum(int portslot);
struct PORTCONTROL * APIENTRY GetPortTableEntryFromSlot(int portslot);


//	Returns number of free buffers
//	(BPQHOST function 7 (part)).

int APIENTRY GetFreeBuffs();

//	Returns count of packets waiting on stream
//	 (BPQHOST function 7 (part)).

int APIENTRY RXCount(int Stream);


//	Returns number of packets on TX queue for stream
//	 (BPQHOST function 7 (part)).

int APIENTRY TXCount(int Stream);


//	Returns number of monitor frames available
//	 (BPQHOST function 7 (part)).

int APIENTRY MONCount(int Stream);


//	Returns call connecten on stream (BPQHOST function 8 (part)).

int APIENTRY GetCallsign(int stream, char * callsign);


//	Returns connection info for stream (BPQHOST function 8).

int APIENTRY GetConnectionInfo(int stream, char * callsign,
										 int * port, int * sesstype, int * paclen,
										 int * maxframe, int * l4window);


int APIENTRY GetStreamPID(int Stream);

// Returns Path of BPQDirectroy

UCHAR * APIENTRY GetBPQDirectory();
UCHAR * APIENTRY GetProgramDirectory();

HKEY APIENTRY GetRegistryKey();
char * APIENTRY GetRegistryKeyText();

UCHAR * APIENTRY GetSignOnMsg();

UCHAR * APIENTRY GetVersionString();


// Returns number of prcess attached to BPQ32

int APIENTRY GetAttachedProcesses();


//	Send Session Control command (BPQHOST function 6)
//	Command = 0 Connect using APPL MASK IN param
//	Command = 1 Connect
//	Command = 2 Disconect
//	Command = 3 Return to Node

int APIENTRY SessionControl(int stream, int command, int param);



//	Sets Application Flags and Mask for stream. (BPQHOST function 1)
//	Top bit of flags enables monitoring

int APIENTRY SetAppl(int stream, int flags, int mask);


int APIENTRY GetApplMask(int Stream);

int APIENTRY GetApplFlags(int Stream);


BOOL APIENTRY GetAllocationState(int Stream);


//	Get current Session State. Any state changed is ACK'ed
//	automatically. See BPQHOST functions 4 and 5.

int APIENTRY SessionState(int stream, int * state, int * change);

//	Get current Session State. Dont Ack state change
//	See BPQHOST function 4.

int APIENTRY SessionStateNoAck(int stream, int * state);



//	Send message to stream (BPQHOST Function 2)

int APIENTRY SendMsg(int stream, char * msg, int len);



//	Send Raw (KISS mode) frame to port (BPQHOST function 10)

int APIENTRY SendRaw(int port, char * msg, int len);



//	Get message from stream. Returns length, and count of frames
//	still waiting to be collected. (BPQHOST function 3)

int APIENTRY GetMsg(int stream, char * msg, int * len, int * count );


// Perl Version - I couldn't get bpq32.pm to call GetMsg. Returns Lenth

int APIENTRY GetMsgPerl(int stream, char * msg);


//	Get Raw (Trace) data (BPQHOST function 11)

int APIENTRY GetRaw(int stream, char * msg, int * len, int * count );



//	This is not an API function. It is a utility to decode a received
//	monitor frame into ascii text.

int APIENTRY DecodeFrame(char * msg, char * buffer, int Stamp);



//	Sets the tracing options for DecodeFrame. Mask is a bit
//	mask of ports to monitor (ie 101 binary will monitor ports
//	1 and 3). MTX enables monitoring on transmitted frames. MCOM
//	enables monitoring of protocol control frames (eg SABM, UA, RR),
//	as well as info frames.

int APIENTRY SetTraceOptions(long mask, int mtxparam, int mcomparam);
int APIENTRY SetTraceOptionsEx(long mask, int mtxparam, int mcomparam, int monUIOnly);


//	Returns number of first unused BPQHOST stream. If none available,
//	returns 255. See API function 13.

int APIENTRY FindFreeStream();



//	Allocate stream. If stream is already allocated, return nonzero.
//	Otherwise allocate stream, and return zero

int APIENTRY AllocateStream(int stream);



//	Release stream.

int APIENTRY DeallocateStream(int stream);

//	Get number of ports configured

int APIENTRY GetNumberofPorts();


//	Get port number (ports aren't necessarily numbered 1 to n)

int APIENTRY GetPortNumber(int portslot);

UCHAR * APIENTRY GetPortDescription(int portslot, char * Desc);

//	Enable async operation - new to Win32 version of API

int APIENTRY BPQSetHandle(int Stream, HWND hWnd);

int ConvFromAX25(unsigned char * incall, unsigned char * outcall);
BOOL ConvToAX25(unsigned char * callsign, unsigned char * ax25call);
char * APIENTRY GetNodeCall();

int APIENTRY ChangeSessionCallsign(int Stream, unsigned char * AXCall);
int APIENTRY ChangeSessionPaclen(int Stream, int Paclen);

int APIENTRY GetApplNum(int Stream);

char * APIENTRY GetApplCall(int Appl);

char * APIENTRY GetApplAlias(int Appl);

long APIENTRY GetApplQual(int Appl);

char * APIENTRY GetApplNabe(int Appl);

BOOL APIENTRY SetApplCall(int Appl, char * NewCall);

BOOL APIENTRY SetApplAlias(int Appl, char * NewCall);

BOOL APIENTRY SetApplQual(int Appl, int NewQual);


// Routines to support "Minimize to Tray"

BOOL APIENTRY GetMinimizetoTrayFlag();

int APIENTRY AddTrayMenuItem(HWND hWnd, char * Label);

int APIENTRY DeleteTrayMenuItem(HWND hWnd);

BOOL APIENTRY StartMinimizedFlag();

// Log a message to the bpq32 console.
 
int APIENTRY WritetoConsole(char * buff);

// CheckTimer should be called regularly to ensure BPQ32 detects if an application crashes

int APIENTRY CheckTimer();

// CloseBPQ32 is used if you want to close and restart BPQ32 while your application is
// running. This is only relevant of you have Dynamically linked BPQ32

int APIENTRY CloseBPQ32();

int APIENTRY GETBPQAPI();

UINT APIENTRY GETMONDECODE();

VOID APIENTRY RelBuff(VOID * Msg);
//VOID *APIENTRY GetBuff();

VOID APIENTRY CreateOneTimePassword(char * Password, char * KeyPhrase, int TimeOffset); 

BOOL APIENTRY CheckOneTimePassword(char * Password, char * KeyPhrase);

VOID APIENTRY md5 (char *arg, unsigned char * checksum);

int APIENTRY SetupTrayIcon();

BOOL APIENTRY SaveReg(char * KeyIn, HANDLE hFile);

VOID APIENTRY SendChatReport(UINT_PTR ChatReportSocket, char * buff, int txlen);

int APIENTRY CountFramesQueuedOnStream(int Stream);

#else

struct PORTCONTROL * (FAR WINAPI *  GetPortTableEntryFromPortNum) (int portnum);
struct PORTCONTROL * (FAR WINAPI *  GetPortTableEntryFromSlot) (int portslot);

//	API Definitions for Dynamic Load of BPQ32.dll

ULONG (FAR WINAPI * GETBPQAPI)();

ULONG (FAR WINAPI * GETMONDECODE)();

UCHAR * (FAR WINAPI * GetBPQDirectory)();
UCHAR * (FAR WINAPI * GetProgramDirectory)();
UCHAR * (FAR WINAPI * GetRegistryKeyText)();
HKEY (FAR WINAPI * GetRegistryKey)();

UCHAR * (FAR WINAPI * GetSignOnMsg)();
UCHAR * (FAR WINAPI * GetVersionString)();

//	Returns number of free buffers
//	(BPQHOST function 7 (part)).

int (FAR WINAPI * GetFreeBuffs)();


//	Returns count of packets waiting on stream
//	 (BPQHOST function 7 (part)).

int (FAR WINAPI *  RXCount) (int Stream);

//	Returns number of packets on TX queue for stream
//	 (BPQHOST function 7 (part)).

int (FAR WINAPI * TXCount)(int Stream);


//	Returns number of monitor frames available
//	 (BPQHOST function 7 (part)).

int (FAR WINAPI * MONCount) (int Stream);

//	Returns call connecten on stream (BPQHOST function 8 (part)).

int (FAR WINAPI * GetCallsign)(int stream, char * callsign);

//	Returns connection info for stream (BPQHOST function 8).

int (FAR WINAPI * GetConnectionInfo) (int stream, char * callsign,
										 int * port, int * sesstype, int * paclen,
										 int * maxframe, int * l4window);


int (FAR WINAPI * GetStreamPID) (int Stream);

// Returns Path of BPQDirectroy

UCHAR * (FAR WINAPI * GetBPQDirectory)();

// Returns number of prcess attached to BPQ32

int (FAR WINAPI * GetAttachedProcesses) ();


//	Send Session Control command (BPQHOST function 6)
//	Command = 0 Connect using APPL MASK IN param
//	Command = 1 Connect
//	Command = 2 Disconect
//	Command = 3 Return to Node

int (FAR WINAPI * SessionControl) (int stream, int command, int param);


//	Sets Application Flags and Mask for stream. (BPQHOST function 1)
//	Top bit of flags enables monitoring

int (FAR WINAPI * SetAppl) (int stream, int flags, int mask);

int (FAR WINAPI * GetApplMask) (int Stream);

int (FAR WINAPI * GetApplFlags)(int Stream);

BOOL (FAR WINAPI * GetAllocationState) (int Stream);



//	Get current Session State. Any state changed is ACK'ed
//	automatically. See BPQHOST functions 4 and 5.

int (FAR WINAPI * SessionState) (int stream, int * state, int * change);

//	Get current Session State. Dont Ack state change
//	See BPQHOST function 4.

int (FAR WINAPI * SessionStateNoAck) (int stream, int * state);


//	Send message to stream (BPQHOST Function 2)

int (FAR WINAPI * SendMsg) (int stream, char * msg, int len);


//	Send Raw (KISS mode) frame to port (BPQHOST function 10)

int (FAR WINAPI * SendRaw) (int port, char * msg, int len);


//	Get message from stream. Returns length, and count of frames
//	still waiting to be collected. (BPQHOST function 3)

int (FAR WINAPI * GetMsg) (int stream, char * msg, int * len, int * count );


// Perl Version - I couldn't get bpq32.pm to call GetMsg. Returns Lenth

int (FAR WINAPI * GetMsgPerl) (int stream, char * msg);


//	Get Raw (Trace) data (BPQHOST function 11)

int (FAR WINAPI * GetRaw) (int stream, char * msg, int * len, int * count );



//	This is not an API function. It is a utility to decode a received
//	monitor frame into ascii text.

int (FAR WINAPI * DecodeFrame) (char * msg, char * buffer, int Stamp);


//	Sets the tracing options for DecodeFrame. Mask is a bit
//	mask of ports to monitor (ie 101 binary will monitor ports
//	1 and 3). MTX enables monitoring on transmitted frames. MCOM
//	enables monitoring of protocol control frames (eg SABM, UA, RR),
//	as well as info frames.

int (FAR WINAPI * SetTraceOptions) (long mask, int mtxparam, int mcomparam);


//	Returns number of first unused BPQHOST stream. If none available,
//	returns 255. See API function 13.

int (FAR WINAPI * FindFreeStream) ();


//	Allocate stream. If stream is already allocated, return nonzero.
//	Otherwise allocate stream, and return zero

int (FAR WINAPI * AllocateStream) (int stream);


//	Release stream.

int (FAR WINAPI * DeallocateStream) (int stream);

//	Get number of ports configured

int (FAR WINAPI * GetNumberofPorts) ();


//	Get port number (ports aren't necessarily numbered 1 to n)

int (FAR WINAPI * GetPortNumber) (int portslot);


//	Enable async operation - new to Win32 version of API

int (FAR WINAPI * BPQSetHandle) (int Stream, HWND hWnd);

int (FAR  * ConvFromAX25) (unsigned char * incall, unsigned char * outcall);
BOOL (FAR  * ConvToAX25) (unsigned char * callsign, unsigned char * ax25call);

char * (FAR WINAPI * GetNodeCall) ();
int (FAR WINAPI * ChangeSessionCallsign) (int Stream, unsigned char * AXCall);

int (FAR WINAPI * GetApplNum) (int Stream);

char * (FAR WINAPI * GetApplCall) (int Appl);

char * (FAR WINAPI * GetApplAlias) (int Appl);

char * (FAR WINAPI * GetApplName) (int Appl);

long (FAR WINAPI * GetApplQual) (int Appl);

BOOL (FAR WINAPI * SetApplCall) (int Appl, char * NewCall);

BOOL (FAR WINAPI * SetApplAlias) (int Appl, char * NewCall);

BOOL (FAR WINAPI * SetApplQual) (int Appl, int NewQual);


// Routines to support "Minimize to Tray"

BOOL (FAR WINAPI * GetMinimizetoTrayFlag)();

//int APIENTRY AddTrayMenuItem();
int (FAR WINAPI * AddTrayMenuItem)(HWND hWnd, char * Label);

//int APIENTRY DeleteTrayMenuItem();
int (FAR WINAPI * DeleteTrayMenuItem)(HWND hWnd);

int (FAR WINAPI * SetupTrayIcon)();

BOOL (FAR WINAPI * GetStartMinimizedFlag)();

// Log a message to the bpq32 console.
 
//int APIENTRY WritetoConsole();
int (FAR WINAPI * WritetoConsole)(char * buff);

int (FAR WINAPI * CheckTimer)();

int (FAR WINAPI * CloseBPQ32)();

HMODULE ExtDriver;

BOOL GetAPI()
{
	// This procedure must be called if you are using Dynamic Linking. It loads BPQ32
	//	if necessary, and get the addresses of the API routines

	int err;
	char Msg[256];

#ifdef EXTDLL
	ExtDriver=GetModuleHandle("bpq32.dll");
#else
	ExtDriver=LoadLibrary("bpq32.dll");
#endif

	if (ExtDriver == NULL)
	{
		err=GetLastError();
		wsprintf(Msg,"Error loading bpq32.dll - Error code %d",err);
		
		MessageBox(NULL,Msg,"BPQDEMO",MB_ICONSTOP);

		return(FALSE);
	}

	GetPortTableEntryFromPortNum = (struct PORTCONTROL * (__stdcall *)(int PortSlot))GetProcAddress(ExtDriver,"_GetPortTableEntryFromPortNum@4");
	GetPortTableEntryFromSlot = (struct PORTCONTROL * (__stdcall *)(int PortSlot))GetProcAddress(ExtDriver,"_GetPortTableEntryFromSlot@4");

	GETBPQAPI = (ULONG(__stdcall *)())GetProcAddress(ExtDriver,"_GETBPQAPI@0");
	GETMONDECODE = (ULONG(__stdcall *)())GetProcAddress(ExtDriver,"_GETMONDECODE@0");

	GetFreeBuffs = (int (__stdcall *)(int stream))GetProcAddress(ExtDriver,"_GetFreeBuffs@0");
	TXCount = (int (__stdcall *)(int stream))GetProcAddress(ExtDriver,"_RXCount@4");
	RXCount = (int (__stdcall *)(int stream))GetProcAddress(ExtDriver,"_RXCount@4");
	MONCount = (int (__stdcall *)(int stream))GetProcAddress(ExtDriver,"_MONCount@4");
	GetCallsign = (int (__stdcall *)(int stream, char * callsign))GetProcAddress(ExtDriver,"_GetCallsign@8");
	GetBPQDirectory = (UCHAR *(__stdcall *)())GetProcAddress(ExtDriver,"_GetBPQDirectory@0");
	GetProgramDirectory = (UCHAR *(__stdcall *)())GetProcAddress(ExtDriver,"_GetProgramDirectory@0");

	GetRegistryKey = (HKEY(__stdcall *)())GetProcAddress(ExtDriver,"_GetRegistryKey@0");
	GetRegistryKeyText = (UCHAR *(__stdcall *)())GetProcAddress(ExtDriver,"_GetRegistryKeyText@0");

	GetSignOnMsg = (UCHAR *(__stdcall *)())GetProcAddress(ExtDriver,"_GetSignOnMsg@0");
	GetVersionString = (UCHAR *(__stdcall *)())GetProcAddress(ExtDriver,"_GetVersionString@0");
	GetConnectionInfo = (int (__stdcall *)(int, char *,int *, int *, int *,int *, int *))GetProcAddress(ExtDriver,"_GetConnectionInfo@28");
	GetStreamPID = (int (__stdcall *)(int Stream))GetProcAddress(ExtDriver,"_GetStreamPID@4");
	GetAttachedProcesses = (int (__stdcall *)())GetProcAddress(ExtDriver,"_GetAttachedProcesses@0");
	SessionControl = (int (__stdcall *)(int stream, int command, int param))GetProcAddress(ExtDriver,"_SessionControl@12");
	SetAppl = (int (__stdcall *)(int stream, int flags, int mask))GetProcAddress(ExtDriver,"_SetAppl@12");
	GetApplMask = (int (__stdcall *)(int Stream))GetProcAddress(ExtDriver,"_GetApplMask@4");
	GetApplFlags = (int (__stdcall *)(int Stream))GetProcAddress(ExtDriver,"_GetApplFlags@4");
	GetAllocationState = (int (__stdcall *)(int Stream))GetProcAddress(ExtDriver,"_GetAllocationState@4");

	SessionState = (int (__stdcall *)(int, int *, int *))GetProcAddress(ExtDriver,"_SessionState@12");
	SessionStateNoAck = (int (__stdcall *)(int, int *))GetProcAddress(ExtDriver,"_SessionStateNoAck@8");
	SendMsg = (int (__stdcall *)(int stream, char * msg, int len))GetProcAddress(ExtDriver,"_SendMsg@12");
	SendRaw = (int (__stdcall *)(int port, char * msg, int len))GetProcAddress(ExtDriver,"_SendRaw@12");
	GetMsg = (int (__stdcall *)(int stream, char * msg, int * len, int * count ))GetProcAddress(ExtDriver,"_GetMsg@16");
	GetMsgPerl = (int (__stdcall *)(int stream, char * msg))GetProcAddress(ExtDriver,"_GetMsgPerl@8");
	GetRaw = (int (__stdcall *)(int stream, char * msg, int * len, int * count))GetProcAddress(ExtDriver,"_GetRaw@16");
	DecodeFrame = (int (__stdcall *)(char * msg, char * buffer, int Stamp))GetProcAddress(ExtDriver,"_DecodeFrame@12");
	SetTraceOptions = (int (__stdcall *)(long mask, int mtxparam, int mcomparam))GetProcAddress(ExtDriver,"_SetTraceOptions@12");
	FindFreeStream = (int (__stdcall *)())GetProcAddress(ExtDriver,"_FindFreeStream@0");
	AllocateStream=  (int (__stdcall *)(int Stream))GetProcAddress(ExtDriver,"_AllocateStream@4");
	DeallocateStream = (int (__stdcall *)(int Stream))GetProcAddress(ExtDriver,"_DeallocateStream@4");

	GetNumberofPorts = (int (__stdcall *)())GetProcAddress(ExtDriver,"_GetNumberofPorts@0");
	GetPortNumber = (int (__stdcall *)(int))GetProcAddress(ExtDriver,"_GetPortNumber@4");
	BPQSetHandle = (int (__stdcall *)(int Stream, HWND hWnd))GetProcAddress(ExtDriver,"_BPQSetHandle@8");

	ConvFromAX25 = (int ( *)(unsigned char * incall, unsigned char * outcall))GetProcAddress(ExtDriver,"ConvFromAX25");
	ConvToAX25 = (BOOL ( *)(unsigned char * callsign, unsigned char * ax25call))GetProcAddress(ExtDriver,"ConvToAX25");
	GetNodeCall = (char * (__stdcall *) ())GetProcAddress(ExtDriver,"_GetNodeCall@0");
	ChangeSessionCallsign = (int (__stdcall *) (int Stream, unsigned char * AXCall))GetProcAddress(ExtDriver,"_ChangeSessionCallsign@8");
	GetApplNum = (int (__stdcall *)(int Stream))GetProcAddress(ExtDriver,"_GetApplNum@4");
	GetApplCall = (char * (__stdcall *) (int Appl))GetProcAddress(ExtDriver,"_GetApplCall@4");
	GetApplAlias = (char * (__stdcall *) (int Appl))GetProcAddress(ExtDriver,"_GetApplAlias@4");
	GetApplQual  = (long (__stdcall *)(int Appl))GetProcAddress(ExtDriver,"_GetApplQual@4");
	GetApplName = (char * (__stdcall *) (int Appl))GetProcAddress(ExtDriver,"_GetApplName@4");
	SetApplCall = (BOOL (__stdcall *)(int Appl, char * NewCall))GetProcAddress(ExtDriver,"_SetApplCall@8");
	SetApplAlias = (BOOL (__stdcall *)(int Appl, char * NewCall))GetProcAddress(ExtDriver,"_SetApplAlias@8");
	SetApplQual = (BOOL (__stdcall *)(int Appl, int NewQual))GetProcAddress(ExtDriver,"_SetApplQual@8");
	
	GetMinimizetoTrayFlag = (BOOL (__stdcall *)())GetProcAddress(ExtDriver,"_GetMinimizetoTrayFlag@0");
	AddTrayMenuItem = (int (__stdcall *)(HWND hWnd, char * Label))GetProcAddress(ExtDriver,"_AddTrayMenuItem@8");
	DeleteTrayMenuItem = (int (__stdcall *)(HWND hWnd))GetProcAddress(ExtDriver,"_DeleteTrayMenuItem@4");
	SetupTrayIcon = (BOOL (__stdcall *)())GetProcAddress(ExtDriver,"_SetupTrayIcon@0");
	
	GetStartMinimizedFlag = (BOOL (__stdcall *)())GetProcAddress(ExtDriver,"_GetStartMinimizedFlag@0");
	WritetoConsole = (int (__stdcall *)(char *))GetProcAddress(ExtDriver,"_WritetoConsole@4");
	CheckTimer = (int (__stdcall *)(char *))GetProcAddress(ExtDriver,"_CheckTimer@0");
	CloseBPQ32 = (int (__stdcall *)(char *))GetProcAddress(ExtDriver,"_CloseBPQ32@0");
	return TRUE;
}

#endif

//
//	Constants and equates for async operation
//

#ifndef BPQWINMSG
#define BPQWINMSG

static char BPQWinMsg[] = "BPQWindowMessage";
UINT BPQMsg;

//
//	Values returned in lParam of Windows Message
//
#define BPQMonitorAvail 1
#define BPQDataAvail 2
#define BPQStateChange 4

#endif

