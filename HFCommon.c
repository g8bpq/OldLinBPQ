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


#pragma data_seg("_BPQDATA")

#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <time.h>

#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02


#include "kernelresource.h"
#include "CHeaders.h"
#include "tncinfo.h"
#ifndef LINBPQ
#include <commctrl.h>
#endif
//#include <stdlib.h>
#include "bpq32.h"
extern char * PortConfig[33];

HANDLE hInstance;
extern HBRUSH bgBrush;
extern HWND ClientWnd, FrameWnd;
extern int OffsetH, OffsetW;

extern HMENU hMainFrameMenu;
extern HMENU hBaseMenu;
extern HANDLE hInstance;

extern HKEY REGTREE;

extern int Ver[];


int KillTNC(struct TNCINFO * TNC);
int RestartTNC(struct TNCINFO * TNC);

unsigned long _beginthread( void( *start_address )(), unsigned stack_size, int arglist);

char * GetChallengeResponse(char * Call, char *  ChallengeString);

VOID __cdecl Debugprintf(const char * format, ...);

static RECT Rect;

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd

#define WSA_ACCEPT WM_USER + 1
#define WSA_DATA WM_USER + 2
#define WSA_CONNECT WM_USER + 3

int Winmor_Socket_Data(int sock, int error, int eventcode);

struct WL2KInfo * WL2KReports;

int WL2KTimer = 0;

int ModetoBaud[31] = {0,0,0,0,0,0,0,0,0,0,0,			// 0 = 10
					  200,600,3200,600,3200,3200,		// 11 - 16
					  0,0,0,0,0,0,0,0,0,0,0,0,0,600};	// 17 - 30

extern char HFCTEXT[];
extern int HFCTEXTLEN;


extern char WL2KCall[10];
extern char WL2KLoc[7];


VOID MoveWindows(struct TNCINFO * TNC)
{
#ifndef LINBPQ
	RECT rcClient;
	int ClientHeight, ClientWidth;

	GetClientRect(TNC->hDlg, &rcClient); 

	ClientHeight = rcClient.bottom;
	ClientWidth = rcClient.right;

	if (TNC->hMonitor)
		MoveWindow(TNC->hMonitor,2 , 185, ClientWidth-4, ClientHeight-187, TRUE);
#endif
}

char * Config;
static char * ptr1, * ptr2;

#ifndef LINBPQ

LRESULT CALLBACK PacWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	MINMAXINFO * mmi;

	int i;
	struct TNCINFO * TNC;

	HKEY hKey;
	char Key[80];
	int retCode, disp;

	for (i=1; i<33; i++)
	{
		TNC = TNCInfo[i];
		if (TNC == NULL)
			continue;
		
		if (TNC->hDlg == hWnd)
			break;
	}

	if (TNC == NULL)
			return DefMDIChildProc(hWnd, message, wParam, lParam);

	switch (message) { 

	case WM_CREATE:

		break;

	case WM_PAINT:

//			hdc = BeginPaint (hWnd, &ps);
			
//			SelectObject( hdc, hFont) ;
			
//			EndPaint (hWnd, &ps);
//
//			wParam = hdc;
	
			break;        


	case WM_GETMINMAXINFO:

 		if (TNC->ClientHeight)
		{
			mmi = (MINMAXINFO *)lParam;
			mmi->ptMaxSize.x = TNC->ClientWidth;
			mmi->ptMaxSize.y = TNC->ClientHeight;
			mmi->ptMaxTrackSize.x = TNC->ClientWidth;
			mmi->ptMaxTrackSize.y = TNC->ClientHeight;
		}

		break;


	case WM_MDIACTIVATE:
	{
			 
		// Set the system info menu when getting activated
			 
		if (lParam == (LPARAM) hWnd)
		{
			// Activate

			RemoveMenu(hBaseMenu, 1, MF_BYPOSITION);

			if (TNC->hMenu)
				AppendMenu(hBaseMenu, MF_STRING + MF_POPUP, (UINT)TNC->hMenu, "Actions");
			
			SendMessage(ClientWnd, WM_MDISETMENU, (WPARAM) hBaseMenu, (LPARAM) hWndMenu);

//			SendMessage(ClientWnd, WM_MDISETMENU, (WPARAM) TNC->hMenu, (LPARAM) TNC->hWndMenu);
		}
		else
		{
			 // Deactivate
	
			SendMessage(ClientWnd, WM_MDISETMENU, (WPARAM) hMainFrameMenu, (LPARAM) NULL);
		 }
			 
		// call DrawMenuBar after the menu items are set
		DrawMenuBar(FrameWnd);

		return DefMDIChildProc(hWnd, message, wParam, lParam);
	}



	case WM_INITMENUPOPUP:

		if (wParam == (WPARAM)TNC->hMenu)
		{
			if (TNC->ProgramPath)
			{
				if (strstr(TNC->ProgramPath, " TNC"))
				{
					EnableMenuItem(TNC->hMenu, WINMOR_RESTART, MF_BYCOMMAND | MF_ENABLED);
					EnableMenuItem(TNC->hMenu, WINMOR_KILL, MF_BYCOMMAND | MF_ENABLED);
		
					break;
				}
			}
			EnableMenuItem(TNC->hMenu, WINMOR_RESTART, MF_BYCOMMAND | MF_GRAYED);
			EnableMenuItem(TNC->hMenu, WINMOR_KILL, MF_BYCOMMAND | MF_GRAYED);
		}
			
		break;

	case WM_COMMAND:

		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch (wmId)
		{
		case WINMOR_KILL:

			KillTNC(TNC);
			break;

		case WINMOR_RESTART:

			KillTNC(TNC);
			RestartTNC(TNC);
			break;

		case WINMOR_RESTARTAFTERFAILURE:

			TNC->RestartAfterFailure = !TNC->RestartAfterFailure;
			CheckMenuItem(TNC->hMenu, WINMOR_RESTARTAFTERFAILURE, (TNC->RestartAfterFailure) ? MF_CHECKED : MF_UNCHECKED);

			sprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\PACTOR\\PORT%d", TNC->Port);
	
			retCode = RegCreateKeyEx(REGTREE, Key, 0, 0, 0, KEY_ALL_ACCESS, NULL, &hKey, &disp);

			if (retCode == ERROR_SUCCESS)
			{
				RegSetValueEx(hKey,"TNC->RestartAfterFailure",0,REG_DWORD,(BYTE *)&TNC->RestartAfterFailure, 4);
				RegCloseKey(hKey);
			}
			break;
		}
		return DefMDIChildProc(hWnd, message, wParam, lParam);

	case WM_SIZING:
	case WM_SIZE:

		MoveWindows(TNC);
			
		return DefMDIChildProc(hWnd, message, wParam, lParam);

	case WM_SYSCOMMAND:

		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		
		switch (wmId)
		{ 

		case SC_RESTORE:

			TNC->Minimized = FALSE;
			break;

		case SC_MINIMIZE: 

			TNC->Minimized = TRUE;
			break;
		}
		
		return DefMDIChildProc(hWnd, message, wParam, lParam);

	case WM_CTLCOLORDLG:
		return (LONG)bgBrush;

		 
	case WM_CTLCOLORSTATIC:
	{
			HDC hdcStatic = (HDC)wParam;
			SetTextColor(hdcStatic, RGB(0, 0, 0));
			SetBkMode(hdcStatic, TRANSPARENT);
			return (LONG)bgBrush;
	}

	case WM_DESTROY:
		
		break;
	}
	return DefMDIChildProc(hWnd, message, wParam, lParam);
}
#endif

BOOL CreatePactorWindow(struct TNCINFO * TNC, char * ClassName, char * WindowTitle, int RigControlRow, WNDPROC WndProc, int Width, int Height)
{
#ifdef LINBPQ
	return FALSE;
#else
    WNDCLASS  wc;
	char Title[80];
	int retCode, Type, Vallen;
	HKEY hKey=0;
	char Key[80];
	char Size[80];
	int Top, Left;
	HANDLE hDlg = 0;
	static int LP = 1235;

	if (TNC->hDlg)
	{
		ShowWindow(TNC->hDlg, SW_SHOWNORMAL);
		SetForegroundWindow(TNC->hDlg);
		return FALSE;							// Already open
	}

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;
    wc.lpfnWndProc = WndProc;                                      
    wc.cbClsExtra = 0;                
    wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = hInstance;
    wc.hIcon = LoadIcon( hInstance, MAKEINTRESOURCE(BPQICON) );
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = bgBrush; 
	wc.lpszMenuName = NULL;	
	wc.lpszClassName = ClassName; 

	RegisterClass(&wc);

	if (TNC->Hardware == H_WINMOR || TNC->Hardware == H_TELNET ||TNC->Hardware == H_ARDOP ||
			TNC->Hardware == H_V4 || TNC->Hardware == H_FLDIGI || TNC->Hardware == H_UIARQ)
		sprintf(Title, "%s Status - Port %d", WindowTitle, TNC->Port);
	else if (TNC->Hardware == H_UZ7HO)
		sprintf(Title, "Rigcontrol for UZ7HO Port %d", TNC->Port);
	else if (TNC->Hardware == H_MPSK)
		sprintf(Title, "Rigcontrol for MultiPSK Port %d", TNC->Port);
	else
		sprintf(Title,"%s Status - %s", WindowTitle, TNC->PortRecord->PORTCONTROL.SerialPortName);


	TNC->hDlg = hDlg =  CreateMDIWindow(ClassName, Title, 0,
		  0, 0, Width, Height, ClientWnd, hInstance, ++LP);
	
	//	CreateDialog(hInstance,ClassName,0,NULL);
	

	sprintf(Key, "SOFTWARE\\G8BPQ\\BPQ32\\PACTOR\\PORT%d", TNC->Port);
	
	retCode = RegOpenKeyEx (REGTREE, Key, 0, KEY_QUERY_VALUE, &hKey);

	if (retCode == ERROR_SUCCESS)
	{
		Vallen=80;

		retCode = RegQueryValueEx(hKey,"Size",0,			
			(ULONG *)&Type,(UCHAR *)&Size,(ULONG *)&Vallen);

		if (retCode == ERROR_SUCCESS)
		{
			sscanf(Size,"%d,%d,%d,%d,%d",&Rect.left,&Rect.right,&Rect.top,&Rect.bottom, &TNC->Minimized);

			if (Rect.top < - 500 || Rect.left < - 500)
			{
				Rect.left = 0;
				Rect.top = 0;
				Rect.right = 600;
				Rect.bottom = 400;
			}

			if (Rect.top < OffsetH)
			{
				int Error = OffsetH - Rect.top;
				Rect.top += Error;
				Rect.bottom += Error;
			}
		}

		if (TNC->Hardware == H_WINMOR || TNC->Hardware == H_ARDOP)	
			retCode = RegQueryValueEx(hKey,"TNC->RestartAfterFailure",0,			
				(ULONG *)&Type,(UCHAR *)&TNC->RestartAfterFailure,(ULONG *)&Vallen);

		RegCloseKey(hKey);
	}

	Top = Rect.top;
	Left = Rect.left;

//	GetWindowRect(hDlg, &Rect);	// Get the real size

	MoveWindow(hDlg, Left - (OffsetW /2), Top - OffsetH, Rect.right - Rect.left, Rect.bottom - Rect.top, TRUE);
	
	if (TNC->Minimized)
		ShowWindow(hDlg, SW_SHOWMINIMIZED);
	else
		ShowWindow(hDlg, SW_RESTORE);

	TNC->RigControlRow = RigControlRow;

	SetWindowText(TNC->xIDC_TNCSTATE, "Free");

	return TRUE;
#endif
}


// WL2K Reporting Code.

static SOCKADDR_IN sinx; 


VOID SendReporttoWL2KThread();
VOID SendHTTPReporttoWL2KThread();

VOID CheckWL2KReportTimer()
{
	if (WL2KReports == NULL)
		return;					// Shouldn't happen!

	WL2KTimer--;

	if (WL2KTimer != 0)
		return;

	WL2KTimer = 32910;			// Every Hour
		
	if (CheckAppl(NULL, "RMS         ") == NULL)
		if (CheckAppl(NULL, "RELAY       ") == NULL)
			return;

	_beginthread(SendHTTPReporttoWL2KThread, 0, 0);

	return;
}

static char HeaderTemplate[] = "POST %s HTTP/1.1\r\n"
	"Accept: application/json\r\n"
//	"Accept-Encoding: gzip,deflate,gzip, deflate\r\n"
	"Content-Type: application/json\r\n"
	"Host: %s:%d\r\n"
	"Content-Length: %d\r\n"
	//r\nUser-Agent: BPQ32(G8BPQ)\r\n"
//	"Expect: 100-continue\r\n"
	"\r\n{%s}";

char Missing[] = "** Missing **";

VOID GetJSONValue(char * _REPLYBUFFER, char * Name, char * Value)
{
	char * ptr1, * ptr2;

	strcpy(Value, Missing);

	ptr1 = strstr(_REPLYBUFFER, Name);

	if (ptr1 == 0)
		return;

	ptr1 += (strlen(Name) + 1);

	ptr2 = strchr(ptr1, '"');
			
	if (ptr2)
	{
		int ValLen = ptr2 - ptr1;
		memcpy(Value, ptr1, ValLen);
		Value[ValLen] = 0;
	}

	return;
}


VOID SendHTTPRequest(SOCKET sock, char * Host, int Port, char * Request, char * Params, int Len, char * Return)
{
	int InputLen = 0;
	int inptr = 0;
	char Buffer[2048];
	char Header[2048];
	char * ptr, * ptr1;

	sprintf(Header, HeaderTemplate, Request, Host, Port, Len + 2, Params);
	send(sock, Header, strlen(Header), 0);

	while (InputLen != -1)
	{
		InputLen = recv(sock, &Buffer[inptr], 2048 - inptr, 0);

		//	As we are using a persistant connection, can't look for close. Check
		//	for complete message

		inptr += InputLen;
		
		ptr = strstr(Buffer, "\r\n\r\n");

		if (ptr)
		{
			// got header

			int Hddrlen = ptr - Buffer;
					
			ptr1 = strstr(Buffer, "Content-Length:");

			if (ptr1)
			{
				// Have content length

				int ContentLen = atoi(ptr1 + 16);

				if (ContentLen + Hddrlen + 4 == inptr)
				{
					// got whole response

					if (strstr(Buffer, " 200 OK"))
					{
						if (Return)
						{
							memcpy(Return, ptr + 4, ContentLen); 
							Return[ContentLen] = 0;
						}
						else
							Debugprintf("WL2K Database update ok");
					}
					return;
				}
			}
		}
	}
}

VOID SendHTTPReporttoWL2KThread()
{
	// Uses HTTP/JSON Interface

	struct WL2KInfo * WL2KReport = WL2KReports;
	char * LastHost = NULL;
	char * LastRMSCall = NULL;
	char Message[256];
	int LastSocket = 0;
	SOCKET sock = 0;
	struct sockaddr_in destaddr;
	int addrlen=sizeof(sinx);
	struct hostent * HostEnt;
	int err;
	u_long param=1;
	BOOL bcopt=TRUE;
	int Len;

	// Send all reports in list

	while (WL2KReport)
	{
		// Resolve Name if needed

		if (LastHost && strcmp(LastHost, WL2KReport->Host) == 0)		// Same host?
			goto SameHost;

		// New Host - Connect to it
	
		LastHost = WL2KReport->Host;
	
		destaddr.sin_family = AF_INET; 
		destaddr.sin_addr.s_addr = inet_addr(WL2KReport->Host);
		destaddr.sin_port = htons(WL2KReport->WL2KPort);

		if (destaddr.sin_addr.s_addr == INADDR_NONE)
		{
			//	Resolve name to address

			Debugprintf("Resolving %s", WL2KReport->Host);
			HostEnt = gethostbyname (WL2KReport->Host);
		 
			if (!HostEnt)
			{
				err = WSAGetLastError();

				Debugprintf("Resolve Failed for %s %d %x", WL2KReport->Host, err, err);
				return;			// Resolve failed
			}
	
			memcpy(&destaddr.sin_addr.s_addr,HostEnt->h_addr,4);	
		}

		//   Allocate a Socket entry

		if (sock)
			closesocket(sock);

		sock = socket(AF_INET, SOCK_STREAM, 0);

		if (sock == INVALID_SOCKET)
  	 		return; 

//		ioctlsocket(sock, FIONBIO, &param);
 
		setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char FAR *)&bcopt, 4);

		destaddr.sin_family = AF_INET;

		if (sock == INVALID_SOCKET)
		{
			sock = 0;
			return; 
		}

		setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

		// Connect to Host

		if (connect(sock,(LPSOCKADDR) &destaddr, sizeof(destaddr)) != 0)
		{
			err=WSAGetLastError();

			//
			//	Connect failed
			//

			Debugprintf("Connect Failed");
			closesocket(sock);
			sock = 0;
			break;
		}

	SameHost:

		Len = sprintf(Message,
				"\"Callsign\":\"%s\","
				"\"BaseCallsign\":\"%s\","
				"\"GridSquare\":\"%s\","
				"\"Frequency\":%d,"
				"\"Mode\":%d,"
				"\"Baud\":%d,"
				"\"Power\":%d,"
				"\"Height\":%d,"
				"\"Gain\":%d,"
				"\"Direction\":%d,"
				"\"Hours\":\"%s\","
				"\"ServiceCode\":\"%s\"",

				WL2KReport->RMSCall, WL2KReport->BaseCall, WL2KReport->GridSquare,
				WL2KReport->Freq, WL2KReport->mode, WL2KReport->baud, WL2KReport->power,
				WL2KReport->height, WL2KReport->gain, WL2KReport->direction,
				WL2KReport->Times, WL2KReport->ServiceCode);

		Debugprintf("Sending %s", Message);

		SendHTTPRequest(sock, WL2KReport->Host, WL2KReport->WL2KPort, 
				"/channel/add", Message, Len, NULL);
	
		
		//	Send Version Message


		if (LastRMSCall == NULL || strcmp(WL2KReport->RMSCall, LastRMSCall) != 0)
		{
			int Len;
			
			LastRMSCall = WL2KReport->RMSCall;

	//	"Callsign":"String","Program":"String","Version":"String","Comments":"String"
		
			Len = sprintf(Message, "\"Callsign\":\"%s\",\"Program\":\"BPQ32\","
				"\"Version\":\"%d.%d.%d.%d\",\"Comments\":\"Test Comment\"",
				WL2KReport->RMSCall, Ver[0], Ver[1], Ver[2], Ver[3]);

			Debugprintf("Sending %s", Message);

			SendHTTPRequest(sock, WL2KReport->Host, WL2KReport->WL2KPort, 
				"/version/add", Message, Len, NULL);
		}

		WL2KReport = WL2KReport->Next;
	}

	Sleep(100);
	closesocket(sock);
	sock = 0;

}

struct WL2KInfo * DecodeWL2KReportLine(char *  buf)
{
	//06'<callsign>', '<base callsign>', '<grid square>', <frequency>, <mode>, <baud>, <power>,
	// <antenna height>, <antenna gain>, <antenna direction>, '<hours>', <group reference>, '<service code>'

	 // WL2KREPORT  service, www.winlink.org, 8778, GM8BPQ, IO68VL, 00-23, 144800000, PKT1200, 10, 20, 5, 0, BPQTEST
	
	char * Context;
	char * p_cmd;
	char * param;
	char errbuf[256];
	struct WL2KInfo * WL2KReport = zalloc(sizeof(struct WL2KInfo));
	char * ptr;


	strcpy(errbuf, buf); 

	p_cmd = strtok_s(&buf[10], ", \t\n\r", &Context);
	if (p_cmd == NULL) goto BadLine;
	
	strcpy(WL2KReport->ServiceCode, p_cmd);

	p_cmd = strtok_s(NULL, ", \t\n\r", &Context);
	if (p_cmd == NULL) goto BadLine;

	_strlwr(p_cmd);

	if (strstr(p_cmd, "winlink.org"))
		WL2KReport->Host = _strdup("server.winlink.org");
	else
		WL2KReport->Host = _strdup(p_cmd);

	p_cmd = strtok_s(NULL, " ,\t\n\r", &Context);			
	if (p_cmd == NULL) goto BadLine;

	WL2KReport->WL2KPort = atoi(p_cmd);

	if (WL2KReport->WL2KPort == 8778)
		WL2KReport->WL2KPort = 8085;			// HTTP Interface

	if (WL2KReport->WL2KPort == 0) goto BadLine;

	p_cmd = strtok_s(NULL, " ,\t\n\r", &Context);		
	if (p_cmd == NULL) goto BadLine;

	strcpy(WL2KReport->RMSCall, p_cmd);
	strcpy(WL2KReport->BaseCall, p_cmd);
	strlop(WL2KReport->BaseCall, '-');					// Remove any SSID
	
	strcpy(WL2KCall, WL2KReport->BaseCall);				// For SYSOP Update

	p_cmd = strtok_s(NULL, " ,\t\n\r", &Context);		
	if (p_cmd == NULL) goto BadLine;
	if (strlen(p_cmd) != 6) goto BadLine;
	
	strcpy(WL2KReport->GridSquare, p_cmd);
	strcpy(WL2KLoc, p_cmd);

	p_cmd = strtok_s(NULL, " ,\t\n\r", &Context);
	if (p_cmd == NULL) goto BadLine;
	if (strlen(p_cmd) > 79) goto BadLine;
	
	// Convert any : in times to comma

	ptr = strchr(p_cmd, ':');

	while (ptr)
	{
		*ptr = ',';
		ptr = strchr(p_cmd, ':');
	}

	strcpy(WL2KReport->Times, p_cmd);

	p_cmd = strtok_s(NULL, " ,\t\n\r", &Context);
	if (p_cmd == NULL) goto BadLine;

	WL2KReport->Freq = atoi(p_cmd);

	if (WL2KReport->Freq == 0)	// Invalid
		goto BadLine;					

	param = strtok_s(NULL, " ,\t\n\r", &Context);

	// Mode Designator - one of

	// PKTnnnnnn
	// WINMOR500
	// WINMOR1600
	// ROBUST
	// P1 P12 P123 P1234 etc

	if (memcmp(param, "PKT", 3) == 0)
	{
		int Speed, Mode;

		Speed = atoi(&param[3]);

		 WL2KReport->baud = Speed;
			
		 if (Speed <= 1200)
			 Mode = 0;					// 1200
		 else if (Speed <= 2400)
			 Mode = 1;					// 2400
		 else if (Speed <= 4800)
			 Mode = 2;					// 4800
		 else if (Speed <= 9600)
			 Mode = 3;					// 9600
		 else if (Speed <= 19200)
			 Mode = 4;					// 19200
		 else if (Speed <= 38400)
			 Mode = 5;					// 38400
		 else
			 Mode = 6;					// >38400

		WL2KReport->mode = Mode;
	}
	else if (_stricmp(param, "WINMOR500") == 0)
		WL2KReport->mode = 21;
	else if (_stricmp(param, "WINMOR1600") == 0)
		WL2KReport->mode = 22;
	else if (_stricmp(param, "ROBUST") == 0)
	{
		WL2KReport->mode = 30;
		WL2KReport->baud = 600;
	}
	else if (_stricmp(param, "ARDOP200") == 0)
		WL2KReport->mode = 40;
	else if (_stricmp(param, "ARDOP500") == 0)
		WL2KReport->mode = 41;
	else if (_stricmp(param, "ARDOP1000") == 0)
		WL2KReport->mode = 42;
	else if (_stricmp(param, "ARDOP2000") == 0)
		WL2KReport->mode = 43;
	else if (_stricmp(param, "ARDOP2000FM") == 0)
		WL2KReport->mode = 44;
	else if (_stricmp(param, "P1") == 0)
		WL2KReport->mode = 11;
	else if (_stricmp(param, "P12") == 0)
		WL2KReport->mode = 12;
	else if (_stricmp(param, "P123") == 0)
		WL2KReport->mode = 13;
	else if (_stricmp(param, "P2") == 0)
		WL2KReport->mode = 14;
	else if (_stricmp(param, "P23") == 0)
		WL2KReport->mode = 15;
	else if (_stricmp(param, "P3") == 0)
		WL2KReport->mode = 16;
	else if (_stricmp(param, "P1234") == 0)
		WL2KReport->mode = 17;
	else if (_stricmp(param, "P234") == 0)
		WL2KReport->mode = 18;
	else if (_stricmp(param, "P34") == 0)
		WL2KReport->mode = 19;
	else if (_stricmp(param, "P4") == 0)
		WL2KReport->mode = 20;
	else
		goto BadLine;
	
	param = strtok_s(NULL, " ,\t\n\r", &Context);

	// Optional Params

	WL2KReport->power = (param)? atoi(param) : 0;
	param = strtok_s(NULL, " ,\t\n\r", &Context);
	WL2KReport->height = (param)? atoi(param) : 0;
	param = strtok_s(NULL, " ,\t\n\r", &Context);
	WL2KReport->gain = (param)? atoi(param) : 0;
	param = strtok_s(NULL, " ,\t\n\r", &Context);
	WL2KReport->direction = (param)? atoi(param) : 0;

	WL2KTimer = 60;

	WL2KReport->Next = WL2KReports;
	WL2KReports = WL2KReport;

	return WL2KReport;

BadLine:
	WritetoConsole(" Bad config record ");
	WritetoConsole(errbuf);
	WritetoConsole("\r\n");

	return 0;
}

VOID UpdateMH(struct TNCINFO * TNC, UCHAR * Call, char Mode, char Direction)
{
	PMHSTRUC MH = TNC->PortRecord->PORTCONTROL.PORTMHEARD;
	PMHSTRUC MHBASE = MH;
	UCHAR AXCall[8];
	int i;
	char * LOC, *  LOCEND;
	char ReportMode[20];
	char NoLOC[7] = "";
	double Freq;
	char ReportFreq[350] = "";

	if (MH == 0) return;

	ConvToAX25(Call, AXCall);
	AXCall[6] |= 1;					// Set End of address

	// Adjust freq to centre

//	if (Mode != ' ' && TNC->RIG->Valchar[0])
	if (TNC->RIG->Valchar[0])
	{
		Freq = atof(TNC->RIG->Valchar) + 0.0015;
		_gcvt(Freq, 9, ReportFreq);
	}

	if (TNC->Hardware == H_ARDOP)	
	{
		LOC = memchr(Call, '[', 20);

		if (LOC)
		{
			LOCEND = memchr(Call, ']', 30);
			if (LOCEND)
			{
				LOC--;
				*(LOC++) = 0;
				*(LOCEND) = 0;
				LOC++;
				if (strlen(LOC) != 6 && strlen(LOC) != 0)
				{
					Debugprintf("Corrupt LOC %s %s", Call, LOC);
					LOC = NoLOC;
				}
				goto NOLOC;
			}		
		}
	}

	else if (TNC->Hardware != H_WINMOR)			// Only WINMOR has a locator
	{
		LOC = NoLOC;
		goto NOLOC;
	}


	LOC = memchr(Call, '(', 20);

	if (LOC)
	{
		LOCEND = memchr(Call, ')', 30);
		if (LOCEND)
		{
			LOC--;
			*(LOC++) = 0;
			*(LOCEND) = 0;
			LOC++;
			if (strlen(LOC) != 6 && strlen(LOC) != 0)
			{
				Debugprintf("Corrupt LOC %s %s", Call, LOC);
				LOC = NoLOC;
			}
		}		
	}
	else
		LOC = NoLOC;

NOLOC:

	for (i = 0; i < MHENTRIES; i++)
	{
		if (Mode == ' ' || Mode == '*')			// Packet
		{
			if ((MH->MHCALL[0] == 0) || ((memcmp(AXCall, MH->MHCALL, 7) == 0) && MH->MHDIGI == Mode)) // Spare or our entry
				goto DoMove;
		}
		else
		{
			if ((MH->MHCALL[0] == 0) || ((memcmp(AXCall, MH->MHCALL, 7) == 0) &&
				MH->MHDIGI == Mode && strcmp(MH->MHFreq, ReportFreq) == 0)) // Spare or our entry
				goto DoMove;
		}
		MH++;
	}

	//	TABLE FULL AND ENTRY NOT FOUND - MOVE DOWN ONE, AND ADD TO TOP

	i = MHENTRIES - 1;
		
	// Move others down and add at front
DoMove:
	if (i != 0)				// First
		memmove(MHBASE + 1, MHBASE, i * sizeof(MHSTRUC));

	memcpy (MHBASE->MHCALL, AXCall, 7);
	MHBASE->MHDIGI = Mode;
	MHBASE->MHTIME = time(NULL);

	memcpy(MHBASE->MHLocator, LOC, 6);
	strcpy(MHBASE->MHFreq, ReportFreq);

	// Report to NodeMap

	if (Mode == '*')
		return;							// Digi'ed Packet
	
	if (Mode == ' ') 					// Packet Data
	{
		if (TNC->PktUpdateMap == 1)
			Mode = '!';
		else	
			return;
	}
			
	ReportMode[0] = TNC->Hardware + '@';
	ReportMode[1] = Mode;
	if (TNC->Hardware == H_HAL)
		ReportMode[2] = TNC->CurrentMode; 
	else
		ReportMode[2] = (TNC->RIG->CurrentBandWidth) ? TNC->RIG->CurrentBandWidth : '?';
	ReportMode[3] = Direction;
	ReportMode[4] = 0;

	SendMH(TNC->Hardware, Call, ReportFreq, LOC, ReportMode);

	return;
}

VOID CloseDriverWindow(int port)
{
#ifndef LINBPQ

	struct TNCINFO * TNC;

	TNC = TNCInfo[port];
	if (TNC == NULL)
		return;

	if (TNC->hDlg == NULL)
		return;

	PostMessage(TNC->hDlg, WM_CLOSE,0,0);
//	DestroyWindow(TNC->hDlg);

	TNC->hDlg = NULL;
#endif
	return;
}

VOID SaveWindowPos(int port)
{
#ifndef LINBPQ

	struct TNCINFO * TNC;
	char Key[80];

	TNC = TNCInfo[port];

	if (TNC == NULL)
		return;

	if (TNC->hDlg == NULL)
		return;
	
	sprintf(Key, "PACTOR\\PORT%d", port);

	SaveMDIWindowPos(TNC->hDlg, Key, "Size", TNC->Minimized);

#endif
	return;
}

VOID ShowTraffic(struct TNCINFO * TNC)
{
	char Status[80];

	sprintf(Status, "RX %d TX %d ACKED %d ", 
		TNC->Streams[0].BytesRXed, TNC->Streams[0].BytesTXed, TNC->Streams[0].BytesAcked);
#ifndef LINBPQ
	SetDlgItemText(TNC->hDlg, IDC_TRAFFIC, Status);
#endif
}

BOOL InterlockedCheckBusy(struct TNCINFO * ThisTNC)
{
	// See if this port, or any interlocked ports are reporting channel busy

	struct TNCINFO * TNC;
	int i, Interlock = ThisTNC->Interlock;

	if (ThisTNC->Busy)
		return TRUE;				// Our port is busy
		
	if (Interlock == 0)
		return ThisTNC->Busy;		// No Interlock

	for (i=1; i<33; i++)
	{
		TNC = TNCInfo[i];
	
		if (TNC == NULL)
			continue;

		if (TNC == ThisTNC)
			continue;

		if (Interlock == TNC->Interlock)	// Same Group	
			if (TNC->Busy)
				return TRUE;				// Interlocked port is busy

	}
	return FALSE;					// None Busy
}

char ChallengeResponse[13];

char * GetChallengeResponse(char * Call, char *  ChallengeString)
{
	// Generates a response to the CMS challenge string...

	long long Challenge = _atoi64(ChallengeString);
	long long CallSum = 0;
	long long Mask;
	long long Response;
	long long XX = 1065484730;

	char CallCopy[10];
	UINT i;


	if (Challenge == 0)
		return "000000000000";

// Calculate Mask from Callsign

	memcpy(CallCopy, Call, 10);
	strlop(CallCopy, '-');
	strlop(CallCopy, ' ');

	for (i = 0; i < strlen(CallCopy); i++)
	{
		CallSum += CallCopy[i];
	}
	
	Mask = CallSum + CallSum * 4963 + CallSum * 782386;

	Response = (Challenge % 930249781);
	Response ^= Mask;

	sprintf(ChallengeResponse, "%012lld", Response);

	return ChallengeResponse;
}

SOCKET OpenWL2KHTTPSock()
{
	SOCKET sock = 0;
	struct sockaddr_in destaddr;
	struct sockaddr_in sinx; 
	int addrlen=sizeof(sinx);
	struct hostent * HostEnt;
	int err;
	u_long param=1;
	BOOL bcopt=TRUE;
		
	destaddr.sin_family = AF_INET; 
	destaddr.sin_port = htons(8085);

	//	Resolve name to address

	HostEnt = gethostbyname ("server.winlink.org");
		 
	if (!HostEnt)
	{
		err = WSAGetLastError();

		Debugprintf("Resolve Failed for %s %d %x", "server.winlink.org", err, err);
		return 0 ;			// Resolve failed
	}
	
	memcpy(&destaddr.sin_addr.s_addr,HostEnt->h_addr,4);	
	
	//   Allocate a Socket entry

	sock = socket(AF_INET,SOCK_STREAM,0);

	if (sock == INVALID_SOCKET)
  	 	return 0; 
 
	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(sock, (struct sockaddr *) &sinx, addrlen) != 0 )
  	 	return FALSE; 

	if (connect(sock,(struct sockaddr *) &destaddr, sizeof(destaddr)) != 0)
	{
		err=WSAGetLastError();
		closesocket(sock);
		return 0;
	}

	return sock;
}

BOOL GetWL2KSYSOPInfo(char * Call, char * _REPLYBUFFER)
{
	SOCKET sock = 0;
	int Len;
	char Message[1000];
		
	sock = OpenWL2KHTTPSock();

	if (sock == 0)
		return 0;
	
	// {"Callsign":"String"}
			
	Len = sprintf(Message, "\"Callsign\":\"%s\"", Call);
		
	SendHTTPRequest(sock, "server.winlink.org", 8085, 
				"/sysop/get", Message, Len, _REPLYBUFFER);

	closesocket(sock);

	return _REPLYBUFFER[0];
}

BOOL UpdateWL2KSYSOPInfo(char * Call, char * SQL)
{

	SOCKET sock = 0;
	struct sockaddr_in destaddr;
	struct sockaddr_in sinx; 
	int len = 100;
	int addrlen=sizeof(sinx);
	struct hostent * HostEnt;
	int err;
	u_long param=1;
	BOOL bcopt=TRUE;
	char Buffer[1000];
	char SendBuffer[1000];
		
	destaddr.sin_family = AF_INET; 
	destaddr.sin_addr.s_addr = inet_addr("statusreport.winlink.org");
	destaddr.sin_port = htons(8775);

	if (destaddr.sin_addr.s_addr == INADDR_NONE)
	{
		//	Resolve name to address
		HostEnt = gethostbyname ("www.winlink.org");
		 
		if (!HostEnt)
		{
			err = WSAGetLastError();

			Debugprintf("Resolve Failed for %s %d %x", "halifax.winlink.org", err, err);
			return 0 ;			// Resolve failed
		}
	
		memcpy(&destaddr.sin_addr.s_addr,HostEnt->h_addr,4);	
	}

	//   Allocate a Socket entry

	sock = socket(AF_INET,SOCK_STREAM,0);

	if (sock == INVALID_SOCKET)
  	 	return 0; 
 
	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (const char FAR *)&bcopt,4);

	sinx.sin_family = AF_INET;
	sinx.sin_addr.s_addr = INADDR_ANY;
	sinx.sin_port = 0;

	if (bind(sock, (struct sockaddr *) &sinx, addrlen) != 0 )
  	 	return FALSE; 

	if (connect(sock,(struct sockaddr *) &destaddr, sizeof(destaddr)) != 0)
	{
		err=WSAGetLastError();
		closesocket(sock);
		return 0;
	}

	len = recv(sock, &Buffer[0], len, 0);

	len = sprintf(SendBuffer, "02%07d%-12s%s%s", strlen(SQL), Call, GetChallengeResponse(Call, Buffer), SQL);

	send(sock, SendBuffer, len, 0);

	len = 1000;

	len = recv(sock, &Buffer[0], len, 0);

	Buffer[len] = 0;
	Debugprintf(Buffer);

	closesocket(sock);

	return TRUE;

}
