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
//	Rig Control Module
//

// Dec 29 2009

//	Add Scan Control for SCS 

// August 2010

// Fix logic error in Port Initialisation (wasn't always raising RTS and DTR
// Clear RTS and DTR on close

// Fix Kenwood processing of multiple messages in one packet.

// Fix reporting of set errors in scan to the wrong session


// Yaesu List

// FT990 define as FT100


#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T


#include <stdio.h>
#include <stdlib.h>
#include "time.h"

#include "CHeaders.h"
#include "tncinfo.h"
#ifndef LINBPQ
#include <commctrl.h>
#else
char *fcvt(double number, int ndigits, int *decpt, int *sign);  
#endif
#include "bpq32.h"

struct TNCINFO * TNCInfo[34];		// Records are Malloc'd
extern char * RigConfigMsg[35];

int Row = -20;

BOOL RIG_DEBUG = FALSE;

extern struct PORTCONTROL * PORTTABLE;

VOID __cdecl Debugprintf(const char * format, ...);

struct RIGINFO * RigConfig(struct TNCINFO * TNC, char * buf, int Port);
struct RIGPORTINFO * CreateTTYInfo(int port, int speed);
BOOL OpenConnection(int);
BOOL SetupConnection(int);
BOOL RigCloseConnection(struct RIGPORTINFO * PORT);
BOOL RigWriteCommBlock(struct RIGPORTINFO * PORT);
BOOL DestroyTTYInfo(int port);
void CheckRX(struct RIGPORTINFO * PORT);
static OpenRigCOMMPort(struct RIGPORTINFO * PORT, VOID * Port, int Speed);
VOID ICOMPoll(struct RIGPORTINFO * PORT);
VOID ProcessFrame(struct RIGPORTINFO * PORT, UCHAR * rxbuff, int len);
VOID ProcessICOMFrame(struct RIGPORTINFO * PORT, UCHAR * rxbuffer, int Len);
int SendResponse(int Stream, char * Msg);
VOID ProcessYaesuFrame(struct RIGPORTINFO * PORT);
VOID YaesuPoll(struct RIGPORTINFO * PORT);
VOID ProcessYaesuCmdAck(struct RIGPORTINFO * PORT);
VOID ProcessKenwoodFrame(struct RIGPORTINFO * PORT, int Length);
VOID KenwoodPoll(struct RIGPORTINFO * PORT);
VOID DummyPoll(struct RIGPORTINFO * PORT);
VOID SwitchAntenna(struct RIGINFO * RIG, char Antenna);
VOID DoBandwidthandAntenna(struct RIGINFO *RIG, struct ScanEntry * ptr);
VOID SetupScanInterLockGroups(struct RIGINFO *RIG);
VOID ProcessFT100Frame(struct RIGPORTINFO * PORT);
VOID ProcessFT990Frame(struct RIGPORTINFO * PORT);
VOID ProcessFT1000Frame(struct RIGPORTINFO * PORT);
VOID AddNMEAChecksum(char * msg);
VOID ProcessNMEA(struct RIGPORTINFO * PORT, char * NMEAMsg, int len);
VOID COMSetDTR(HANDLE fd);
VOID COMClearDTR(HANDLE fd);
VOID COMSetRTS(HANDLE fd);
VOID COMClearRTS(HANDLE fd);

VOID SetupPortRIGPointers();
VOID PTTCATThread(struct RIGINFO *RIG);

int SendPTCRadioCommand(struct TNCINFO * TNC, char * Block, int Length);
int GetPTCRadioCommand(struct TNCINFO * TNC, char * Block);

extern  TRANSPORTENTRY * L4TABLE;
HANDLE hInstance;

VOID APIENTRY CreateOneTimePassword(char * Password, char * KeyPhrase, int TimeOffset); 
BOOL APIENTRY CheckOneTimePassword(char * Password, char * KeyPhrase);

char * GetApplCallFromName(char * App);

char Modes[20][6] = {"LSB",  "USB", "AM", "CW", "RTTY", "FM", "WFM", "CW-R", "RTTY-R",
					"????","????","????","????","????","????","????","????","DV", "????",};

//							0		1	  2		3	   4	5	6	7	8	9		0A  0B    0C    88

char YaesuModes[16][6] = {"LSB",  "USB", "CW", "CWR", "AM", "", "", "", "FM", "", "DIG", "", "PKT", "FMN", "????"};

char FT100Modes[9][6] = {"LSB",  "USB", "CW", "CWR", "AM", "DIG", "FM", "WFM", "????"};

char FT990Modes[13][6] = {"LSB",  "USB", "CW2k4", "CW500", "AM6k", "AM2k4", "FM", "FM", "RTTYL", "RTTYU", "PKTL", "PKTFM", "????"};

char FT1000Modes[13][6] = {"LSB",  "USB", "CW", "CWR", "AM", "AMS", "FM", "WFM", "RTTYL", "RTTYU", "PKTL", "PKTF", "????"};

char FTRXModes[8][6] = {"LSB", "USB", "CW", "AM", "FM", "RTTY", "PKT", ""};

char KenwoodModes[16][6] = {"????", "LSB",  "USB", "CW", "FM", "AM", "FSK", "????"};

//char FT2000Modes[16][6] = {"????", "LSB",  "USB", "CW", "FM", "AM", "FSK", "PKT-L", "FSK-R", "PKT-FM", "FM-N", "PKT-U", "????"};
char FT2000Modes[16][6] = {"????", "LSB",  "USB", "CW", "FM", "AM", "FSK", "CW-R", "PKT-L", "FSK-R", "PKT-FM", "FM-N", "PKT-U", "????"};

char FLEXModes[16][6] = {"LSB", "USB", "DSB", "CWL", "CWU", "FM", "AM", "DIGU", "SPEC", "DIGL", "SAM", "DRM"};

char AuthPassword[100] = "";

char LastPassword[17];

int NumberofPorts = 0;

BOOL EndPTTCATThread = FALSE;

struct RIGPORTINFO * PORTInfo[34] = {NULL};		// Records are Malloc'd


struct TimeScan * AllocateTimeRec(struct RIGINFO * RIG)
{
	struct TimeScan * Band = malloc(sizeof (struct TimeScan));
	
	RIG->TimeBands = realloc(RIG->TimeBands, (++RIG->NumberofBands+2)*4);
	RIG->TimeBands[RIG->NumberofBands] = Band;
	RIG->TimeBands[RIG->NumberofBands+1] = NULL;

	return Band;
}

struct ScanEntry ** CheckTimeBands(struct RIGINFO * RIG)
{
	int i = 0;
	time_t NOW = time(NULL) % 86400;
				
	// Find TimeBand

	while (i < RIG->NumberofBands)
	{
		if (RIG->TimeBands[i + 1]->Start > NOW)
		{
			break;
		}
		i++;
	}

	RIG->FreqPtr = RIG->TimeBands[i]->Scanlist;

	return RIG->FreqPtr;
}


VOID Rig_PTT(struct RIGINFO * RIG, BOOL PTTState)
{
	struct RIGPORTINFO * PORT;

	if (RIG == NULL) return;

	PORT = RIG->PORT;

	if (PTTState)
	{
		MySetWindowText(RIG->hPTT, "T");
		RIG->WEB_PTT = 'T';
		RIG->PTTTimer = PTTLimit;
	}
	else
	{
		MySetWindowText(RIG->hPTT, "");
		RIG->WEB_PTT = ' ';
		RIG->PTTTimer = 0;
	}

	if (RIG->PTTMode & PTTCI_V)
	{
		UCHAR * Poll = PORT->TXBuffer;

		switch (PORT->PortType)
		{
		case ICOM:
		case KENWOOD:
		case FT2000:
		case FLEX:
		case NMEA:

			if (PTTState)
			{
				memcpy(Poll, RIG->PTTOn, RIG->PTTOnLen);
				PORT->TXLen = RIG->PTTOnLen;
			}
			else
			{
				memcpy(Poll, RIG->PTTOff, RIG->PTTOffLen);
				PORT->TXLen = RIG->PTTOffLen;
			}

			RigWriteCommBlock(PORT);

			if (PORT->PortType == ICOM && !PTTState)
				RigWriteCommBlock(PORT); // Send ICOP PTT OFF Twice

			PORT->Retries = 1;
			
			if (PORT->PortType != ICOM)
				PORT->Timeout = 0;
			
			return;

		case FT100:
		case FT990:
		case FT1000:

			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = PTTState;	// OFF/ON
			*(Poll++) = 15;
	
			PORT->TXLen = 5;
			RigWriteCommBlock(PORT);

			PORT->Retries = 1;
			PORT->Timeout = 0;

			return;

		case YAESU:  // 897 - maybe others

			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = PTTState ? 0x08 : 0x88;		// CMD = 08 : PTT ON CMD = 88 : PTT OFF
	
			PORT->TXLen = 5;
			RigWriteCommBlock(PORT);

			PORT->Retries = 1;
			PORT->Timeout = 0;

			return;

		}
	}

	if (RIG->PTTMode & PTTRTS)
		if (PTTState)
			COMSetRTS(PORT->hPTTDevice);
		else
			COMClearRTS(PORT->hPTTDevice);

	if (RIG->PTTMode & PTTDTR)
		if (PTTState)
			COMSetDTR(PORT->hPTTDevice);
		else
			COMClearDTR(PORT->hPTTDevice);
}

struct RIGINFO * Rig_GETPTTREC(int Port)
{
	struct RIGINFO * RIG;
	struct RIGPORTINFO * PORT;
	int i, p;

	for (p = 0; p < NumberofPorts; p++)
	{
		PORT = PORTInfo[p];

		for (i=0; i< PORT->ConfiguredRigs; i++)
		{
			RIG = &PORT->Rigs[i];

			if (RIG->BPQPort & (1 << Port))
				return RIG;
		}
	}

	return NULL;
}



int Rig_Command(int Session, char * Command)
{
	int n, Port, ModeNo, Filter;
	double Freq = 0.0;
	char FreqString[80]="", FilterString[80]="", Mode[80]="", Data[80] = "";
	UINT * buffptr;
	UCHAR * Poll;
	char * 	Valchar ;
	int dec, sign;
	struct RIGPORTINFO * PORT;
	int i, p;
	struct RIGINFO * RIG;
	TRANSPORTENTRY * L4 = L4TABLE;
	char * ptr;
	int Split, DataFlag, Bandwidth, Antenna;
	struct ScanEntry * FreqPtr;
	char * CmdPtr;
	int Len;
	char MemoryBank = 0;	// For Memory Scanning
	int MemoryNumber = 0;

	//	Only Allow RADIO from Secure Applications

	_strupr(Command);

	ptr = strchr(Command, 13);
	if (ptr) *(ptr) = 0;						// Null Terminate

	if (memcmp(Command, "AUTH ", 5) == 0)
	{
		if (AuthPassword[0] && (memcmp(LastPassword, &Command[5], 16) != 0))
		{
			if (CheckOneTimePassword(&Command[5], AuthPassword))
			{
				L4 += Session;
				L4->Secure_Session = 1;

				sprintf(Command, "Ok\r");

				memcpy(LastPassword, &Command[5], 16);	// Save

				return FALSE;
			}
		}
		
		sprintf(Command, "Sorry AUTH failed\r");
		return FALSE;
	}

	if (Session != -1)				// Used for internal Stop/Start
	{		
		L4 += Session;

		if (L4->Secure_Session == 0)
		{
			sprintf(Command, "Sorry - you are not allowed to use this command\r");
			return FALSE;
		}
	}
	if (NumberofPorts == 0)
	{
		sprintf(Command, "Sorry - Rig Control not configured\r");
		return FALSE;
	}

	n = sscanf(Command,"%d %s %s %s %s", &Port, &FreqString[0], &Mode[0], &FilterString[0], &Data[0]);

	// Look for the port 

	for (p = 0; p < NumberofPorts; p++)
	{
		PORT = PORTInfo[p];

		for (i=0; i< PORT->ConfiguredRigs; i++)
		{
			RIG = &PORT->Rigs[i];

			if (RIG->BPQPort & (1 << Port))
				goto portok;
		}
	}

	sprintf(Command, "Sorry - Port not found\r");
	return FALSE;

portok:

	if (RIG->RIGOK == 0 && Session != -1)
	{
		sprintf(Command, "Sorry - Radio not responding\r");
		return FALSE;
	}

	if (n > 1)
	{
		if (_stricmp(FreqString, "SCANSTART") == 0)
		{
			if (RIG->NumberofBands)
			{
				RIG->ScanStopped &= (0xffffffff ^ (1 << Port));

				if (Session != -1)				// Used for internal Stop/Start
					RIG->ScanStopped &= 0xfffffffe; // Clear Manual Stopped Bit

				if (n > 2)
					RIG->ScanCounter = atoi(Mode) * 10;  //Start Delay
				else
					RIG->ScanCounter = 10;

				RIG->WaitingForPermission = FALSE;		// In case stuck	

				if (RIG->ScanStopped == 0)
				{
					SetWindowText(RIG->hSCAN, "S");
					RIG->WEB_SCAN = 'S';
				}
				sprintf(Command, "Ok\r");

				if (RIG_DEBUG)
					Debugprintf("BPQ32 SCANSTART Port %d", Port);
			}
			else
				sprintf(Command, "Sorry no Scan List defined for this port\r");

			return FALSE;
		}

		if (_stricmp(FreqString, "SCANSTOP") == 0)
		{
			RIG->ScanStopped |= (1 << Port);

			if (Session != -1)				// Used for internal Stop/Start
				RIG->ScanStopped |= 1;		// Set Manual Stopped Bit

			SetWindowText(RIG->hSCAN, "");
			RIG->WEB_SCAN = ' ';

			sprintf(Command, "Ok\r");

			if (RIG_DEBUG)
				Debugprintf("BPQ32 SCANSTOP Port %d", Port);

			return FALSE;
		}
	}

	RIG->Session = Session;		// BPQ Stream

	if (_memicmp(FreqString, "Chan", 4) == 0)
	{
		if (strchr(FreqString, '/')	)	// Bank/Chan
		{
			MemoryBank = FreqString[4];
			MemoryNumber = atoi(&FreqString[6]);
		}
		else
			MemoryNumber = atoi(&FreqString[4]);	// Just Chan

		Freq = 0.0;
	}
	else
	{
		Freq = atof(FreqString);

		if (Freq < 0.1)
		{
			strcpy(Command, "Sorry - Invalid Frequency\r");
			return FALSE;
		}
	}
	Freq = Freq * 1000000.0;

	Valchar = _fcvt(Freq, 0, &dec, &sign);

	if (dec == 9)	// 10-100
		sprintf(FreqString, "%s", Valchar);
	else
	if (dec == 8)	// 10-100
		sprintf(FreqString, "0%s", Valchar);
	else
	if (dec == 7)	// 1-10
		sprintf(FreqString, "00%s", Valchar);
	else
	if (dec == 6)	// 0 - 1
		sprintf(FreqString, "000%s", Valchar);

	if (PORT->PortType != ICOM)
		strcpy(Data, FilterString);			// Others don't have a filter.

	Split = DataFlag = Bandwidth = Antenna = 0;

	_strupr(Data);

	if (strchr(Data, '+'))
		Split = '+';
	else if (strchr(Data, '-'))				
		Split = '-';
	else if (strchr(Data, 'S'))
		Split = 'S';	
	else if (strchr(Data, 'D'))	
		DataFlag = 1;
								
	if (strchr(Data, 'W'))
		Bandwidth = 'W';	
	else if (strchr(Data, 'N'))
		Bandwidth = 'N';

	if (strstr(Data, "A1"))
		Antenna = '1';
	else if (strstr(Data, "A2"))
		Antenna = '2';
	if (strstr(Data, "A3"))
		Antenna = '3';
	else if (strstr(Data, "A4"))
		Antenna = '4';

	switch (PORT->PortType)
	{ 
	case ICOM:

		if (n == 2)
			// Set Freq Only

			ModeNo = -1;
		else
		{
			if (n < 4)
			{
				strcpy(Command, "Sorry - Invalid Format - should be Port Freq Mode Filter Width\r");
				return FALSE;
			}

			Filter = atoi(FilterString);

			for (ModeNo = 0; ModeNo < 8; ModeNo++)
			{
				if (_stricmp(Modes[ModeNo], Mode) == 0)
					break;
			}

			if (ModeNo == 8)
			{
				sprintf(Command, "Sorry - Invalid Mode\r");
				return FALSE;
			}
		}

		buffptr = GetBuff();

		if (buffptr == 0)
		{
			sprintf(Command, "Sorry - No Buffers available\r");
			return FALSE;
		}

		// Build a ScanEntry in the buffer

		FreqPtr = (struct ScanEntry *)&buffptr[2];

		memset(FreqPtr, 0, sizeof(struct ScanEntry));

		FreqPtr->Freq = Freq;
		FreqPtr->Bandwidth = Bandwidth;
		FreqPtr->Antenna = Antenna;
		FreqPtr->Dwell = 51;

		CmdPtr = FreqPtr->Cmd1 = (UCHAR *)&buffptr[20];
		FreqPtr->Cmd2 = NULL;
		FreqPtr->Cmd3 = NULL;

		*(CmdPtr++) = 0xFE;
		*(CmdPtr++) = 0xFE;
		*(CmdPtr++) = RIG->RigAddr;
		*(CmdPtr++) = 0xE0;


		if (MemoryNumber)
		{
			// Set Memory Channel instead of Freq, Mode, etc

			char ChanString[5];

			// Send Set Memory, then Channel
								
			*(CmdPtr++) = 0x08;
			*(CmdPtr++) = 0xFD;

			*(CmdPtr++) = 0xFE;
			*(CmdPtr++) = 0xFE;
			*(CmdPtr++) = RIG->RigAddr;
			*(CmdPtr++) = 0xE0;

			sprintf(ChanString, "%04d", MemoryNumber); 
	
			*(CmdPtr++) = 0x08;
			*(CmdPtr++) = (ChanString[1] - 48) | ((ChanString[0] - 48) << 4);
			*(CmdPtr++) = (ChanString[3] - 48) | ((ChanString[2] - 48) << 4);
			*(CmdPtr++) = 0xFD;
				
			FreqPtr[0].Cmd1Len = 14;

			if (MemoryBank)
			{						
				*(CmdPtr++) = 0xFE;
				*(CmdPtr++) = 0xFE;
				*(CmdPtr++) = RIG->RigAddr;
				*(CmdPtr++) = 0xE0;
				*(CmdPtr++) = 0x08;
				*(CmdPtr++) = 0xA0;
				*(CmdPtr++) = MemoryBank - 0x40;
				*(CmdPtr++) = 0xFD;

				FreqPtr[0].Cmd1Len += 8;
			}	
		}
		else
		{
			*(CmdPtr++) = 0x5;		// Set frequency command

			// Need to convert two chars to bcd digit
	
			*(CmdPtr++) = (FreqString[8] - 48) | ((FreqString[7] - 48) << 4);
			*(CmdPtr++) = (FreqString[6] - 48) | ((FreqString[5] - 48) << 4);
			*(CmdPtr++) = (FreqString[4] - 48) | ((FreqString[3] - 48) << 4);
			*(CmdPtr++) = (FreqString[2] - 48) | ((FreqString[1] - 48) << 4);
			*(CmdPtr++) = (FreqString[0] - 48);

			*(CmdPtr++) = 0xFD;

			FreqPtr[0].Cmd1Len = 11;

			// Send Set VFO in case last chan was memory
							
	//		*(CmdPtr++) = 0xFE;
	//		*(CmdPtr++) = 0xFE;
	//		*(CmdPtr++) = RIG->RigAddr;
	//		*(CmdPtr++) = 0xE0;

	//		*(CmdPtr++) = 0x07;
	//		*(CmdPtr++) = 0xFD;

	//		FreqPtr[0].Cmd1Len = 17;

			if (ModeNo != -1)			// Dont Set
			{		
				CmdPtr = FreqPtr->Cmd2 = (UCHAR *)&buffptr[30];
				*(CmdPtr++) = 0xFE;
				*(CmdPtr++) = 0xFE;
				*(CmdPtr++) = RIG->RigAddr;
				*(CmdPtr++) = 0xE0;
				*(CmdPtr++) = 0x6;		// Set Mode
				*(CmdPtr++) = ModeNo;
				*(CmdPtr++) = Filter;
				*(CmdPtr++) = 0xFD;

				FreqPtr->Cmd2Len = 8;

				if (Split)
				{
					CmdPtr = FreqPtr->Cmd3 = (UCHAR *)&buffptr[40];
					FreqPtr->Cmd3Len = 7;
					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = RIG->RigAddr;
					*(CmdPtr++) = 0xE0;
					*(CmdPtr++) = 0xF;		// Set Mode
					if (Split == 'S')
						*(CmdPtr++) = 0x10;
					else
						if (Split == '+')
							*(CmdPtr++) = 0x12;
					else
						if (Split == '-')
							*(CmdPtr++) = 0x11;
			
					*(CmdPtr++) = 0xFD;
				}
				else if (DataFlag)
				{
					CmdPtr = FreqPtr->Cmd3 = (UCHAR *)&buffptr[40];

					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = RIG->RigAddr;
					*(CmdPtr++) = 0xE0;
					*(CmdPtr++) = 0x1a;	

					if ((strcmp(RIG->RigName, "IC7100") == 0) || (strcmp(RIG->RigName, "IC7410") == 0))				{
						FreqPtr[0].Cmd3Len = 9;
						*(CmdPtr++) = 0x6;		// Send/read DATA mode with filter set
						*(CmdPtr++) = 0x1;		// Data On
						*(CmdPtr++) = Filter;	//Filter
					}
					else if (strcmp(RIG->RigName, "IC7200") == 0)
					{
						FreqPtr[0].Cmd3Len = 9;
						*(CmdPtr++) = 0x4;		// Send/read DATA mode with filter set
						*(CmdPtr++) = 0x1;		// Data On
						*(CmdPtr++) = Filter;	// Filter
					}
					else
					{
						FreqPtr[0].Cmd3Len = 8;
						*(CmdPtr++) = 0x6;		// Set Data
						*(CmdPtr++) = 0x1;		//On		
					}
						
					*(CmdPtr++) = 0xFD;
				}
			}
		}

		buffptr[1] = 200;
		
		C_Q_ADD(&RIG->BPQtoRADIO_Q, buffptr);

		return TRUE;

	case YAESU:
			
		if (n < 3)
		{
			strcpy(Command, "Sorry - Invalid Format - should be Port Freq Mode\r");
			return FALSE;
		}

		for (ModeNo = 0; ModeNo < 15; ModeNo++)
		{
			if (_stricmp(YaesuModes[ModeNo], Mode) == 0)
				break;
		}

		if (ModeNo == 15)
		{
			sprintf(Command, "Sorry -Invalid Mode\r");
			return FALSE;
		}

		buffptr = GetBuff();

		if (buffptr == 0)
		{
			sprintf(Command, "Sorry - No Buffers available\r");
			return FALSE;
		}

		// Build a ScanEntry in the buffer

		FreqPtr = (struct ScanEntry *)&buffptr[2];

		FreqPtr->Freq = Freq;
		FreqPtr->Bandwidth = Bandwidth;
		FreqPtr->Antenna = Antenna;

		Poll = (UCHAR *)&buffptr[20];

		// Send Mode then Freq - setting Mode seems to change frequency

		*(Poll++) = ModeNo;
		*(Poll++) = 0;
		*(Poll++) = 0;
		*(Poll++) = 0;
		*(Poll++) = 7;		// Set Mode

		*(Poll++) = (FreqString[1] - 48) | ((FreqString[0] - 48) << 4);
		*(Poll++) = (FreqString[3] - 48) | ((FreqString[2] - 48) << 4);
		*(Poll++) = (FreqString[5] - 48) | ((FreqString[4] - 48) << 4);
		*(Poll++) = (FreqString[7] - 48) | ((FreqString[6] - 48) << 4);
		*(Poll++) = 1;		// Set Freq
					
		buffptr[1] = 10;

		if (strcmp(PORT->Rigs[0].RigName, "FT847") == 0)
		{
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = 3;		// Status Poll
	
			buffptr[1] = 15;
		}
		
		C_Q_ADD(&RIG->BPQtoRADIO_Q, buffptr);

		return TRUE;


	case FT100:
	case FT990:
	case FT1000:

		if (n == 2)			// Set Freq Only
			ModeNo = -1;
		else
		{
			if (n < 3)
			{
				strcpy(Command, "Sorry - Invalid Format - should be Port Freq Mode\r");
				return FALSE;
			}
		
			if (PORT->PortType == FT100)
			{
				for (ModeNo = 0; ModeNo < 8; ModeNo++)	
				{
					if (_stricmp(FT100Modes[ModeNo], Mode) == 0)
						break;
				}

				if (ModeNo == 8)
				{
					sprintf(Command, "Sorry -Invalid Mode\r");
					return FALSE;
				}
			}
			else if (PORT->PortType == FT990)
			{
				for (ModeNo = 0; ModeNo < 12; ModeNo++)	
				{
					if (_stricmp(FT990Modes[ModeNo], Mode) == 0)
						break;
				}

				if (ModeNo == 12)
				{
					sprintf(Command, "Sorry -Invalid Mode\r");
					return FALSE;
				}
			}
			else
			{
				for (ModeNo = 0; ModeNo < 12; ModeNo++)	
				{
					if (_stricmp(FT1000Modes[ModeNo], Mode) == 0)
						break;
				}

				if (ModeNo == 12)
				{
					sprintf(Command, "Sorry -Invalid Mode\r");
					return FALSE;
				}
			}
		}

		buffptr = GetBuff();

		if (buffptr == 0)
		{
			sprintf(Command, "Sorry - No Buffers available\r");
			return FALSE;
		}

		// Build a ScanEntry in the buffer

		FreqPtr = (struct ScanEntry *)&buffptr[2];

		FreqPtr->Freq = Freq;
		FreqPtr->Bandwidth = Bandwidth;
		FreqPtr->Antenna = Antenna;

		Poll = (UCHAR *)&buffptr[20];

		// Send Mode then Freq - setting Mode seems to change frequency

		if (ModeNo == -1)		// Don't set Mode
		{
			// Changing the length messes up a lot of code,
			// so set freq twice instead of omitting entry

			*(Poll++) = (FreqString[7] - 48) | ((FreqString[6] - 48) << 4);
			*(Poll++) = (FreqString[5] - 48) | ((FreqString[4] - 48) << 4);
			*(Poll++) = (FreqString[3] - 48) | ((FreqString[2] - 48) << 4);
			*(Poll++) = (FreqString[1] - 48) | ((FreqString[0] - 48) << 4);
			*(Poll++) = 10;		// Set Freq
		}
		else
		{
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = 0;
			*(Poll++) = ModeNo;
			*(Poll++) = 12;		// Set Mode
		}

		*(Poll++) = (FreqString[7] - 48) | ((FreqString[6] - 48) << 4);
		*(Poll++) = (FreqString[5] - 48) | ((FreqString[4] - 48) << 4);
		*(Poll++) = (FreqString[3] - 48) | ((FreqString[2] - 48) << 4);
		*(Poll++) = (FreqString[1] - 48) | ((FreqString[0] - 48) << 4);
		*(Poll++) = 10;		// Set Freq

		*(Poll++) = 0;
		*(Poll++) = 0;
		*(Poll++) = 0;
		if (PORT->PortType == FT990 || PORT->YaesuVariant == FT1000D)
			*(Poll++) = 3;
		else
			*(Poll++) = 2;		// 100 or 1000MP

		*(Poll++) = 16;		// Status Poll
	
		buffptr[1] = 15;
		
		C_Q_ADD(&RIG->BPQtoRADIO_Q, buffptr);

		return TRUE;

	case KENWOOD:
	case FT2000:
	case FLEX:
			
		if (n < 3)
		{
			strcpy(Command, "Sorry - Invalid Format - should be Port Freq Mode\r");
			return FALSE;
		}

		for (ModeNo = 0; ModeNo < 14; ModeNo++)
		{
			if (PORT->PortType == FT2000)
				if (_stricmp(FT2000Modes[ModeNo], Mode) == 0)
				break;

			if (PORT->PortType == KENWOOD)
				if (_stricmp(KenwoodModes[ModeNo], Mode) == 0)
				break;
			if (PORT->PortType == FLEX)
				if (_stricmp(FLEXModes[ModeNo], Mode) == 0)
				break;
		}

		if (ModeNo > 12)
		{
			sprintf(Command, "Sorry -Invalid Mode\r");
			return FALSE;
		}

		buffptr = GetBuff();

		if (buffptr == 0)
		{
			sprintf(Command, "Sorry - No Buffers available\r");
			return FALSE;
		}

		// Build a ScanEntry in the buffer

		FreqPtr = (struct ScanEntry *)&buffptr[2];

		FreqPtr->Freq = Freq;
		FreqPtr->Bandwidth = Bandwidth;
		FreqPtr->Antenna = Antenna;

		Poll = (UCHAR *)&buffptr[20];

		if (PORT->PortType == FT2000)
			buffptr[1] = sprintf(Poll, "FA%s;MD0%X;FA;MD;", &FreqString[1], ModeNo);
		else
		if (PORT->PortType == FLEX)
			buffptr[1] = sprintf(Poll, "ZZFA00%s;ZZMD%02d;ZZFA;ZZMD;", &FreqString[1], ModeNo);
		else
			buffptr[1] = sprintf(Poll, "FA00%s;MD%d;FA;MD;", FreqString, ModeNo);
		
		C_Q_ADD(&RIG->BPQtoRADIO_Q, buffptr);

		return TRUE;

	case NMEA:
			
		if (n < 3)
		{
			strcpy(Command, "Sorry - Invalid Format - should be Port Freq Mode\r");
			return FALSE;
		}
		buffptr = GetBuff();

		if (buffptr == 0)
		{
			sprintf(Command, "Sorry - No Buffers available\r");
			return FALSE;
		}

		// Build a ScanEntry in the buffer

		FreqPtr = (struct ScanEntry *)&buffptr[2];

		FreqPtr->Freq = Freq;
		FreqPtr->Bandwidth = Bandwidth;
		FreqPtr->Antenna = Antenna;

		Poll = (UCHAR *)&buffptr[20];

		i = sprintf(Poll, "$PICOA,90,%02x,RXF,%.6f*xx\r\n", RIG->RigAddr, Freq/1000000.);
		AddNMEAChecksum(Poll);
		Len = i;
		i = sprintf(Poll + Len, "$PICOA,90,%02x,TXF,%.6f*xx\r\n", RIG->RigAddr, Freq/1000000.);
		AddNMEAChecksum(Poll + Len);
		Len += i;
		i = sprintf(Poll + Len, "$PICOA,90,%02x,MODE,%s*xx\r\n", RIG->RigAddr, Mode);
		AddNMEAChecksum(Poll + Len);
			
		buffptr[1] = i + Len;
		
		C_Q_ADD(&RIG->BPQtoRADIO_Q, buffptr);

		return TRUE;

	}
	return TRUE;
}

int BittoInt(UINT BitMask)
{
	// Returns bit position of first 1 bit in BitMask
	
	int i = 0;
	while ((BitMask & 1) == 0)
	{	
		BitMask >>= 1;
		i ++;
	}
	return i;
}


DllExport BOOL APIENTRY Rig_Init()
{
	struct RIGPORTINFO * PORT;
	int i, p, port;
	struct RIGINFO * RIG;
	struct TNCINFO * TNC;
	HWND hDlg;
#ifndef LINBPQ
	int RigRow;
#endif
	// Get config info

	NumberofPorts = 0;

	for (port = 0; port < 32; port++)
		PORTInfo[port] = NULL;

	for (port = 1; port < 33; port++)
	{
		TNC = TNCInfo[port];

		if (TNC == NULL)
			continue;

		if (RigConfigMsg[port])
		{
			char msg[1000];
			
			char * SaveRigConfig = _strdup(RigConfigMsg[port]);
			char * RigConfigMsg1 = _strdup(RigConfigMsg[port]);

			RIG = TNC->RIG = RigConfig(TNC, RigConfigMsg1, port);
			
			if (TNC->RIG == NULL)
			{
				// Report Error

				sprintf(msg,"Port %d Invalid Rig Config %s", port, SaveRigConfig);
				WritetoConsole(msg);
				free(SaveRigConfig);
				free(RigConfigMsg1);
				continue;
			}
		
			TNC->RIG->PTTMode = TNC->PTTMode;

			if (TNC->RIG->PTTMode == 0)			// Not Set
				TNC->RIG->PTTMode = PTTCI_V;	// For PTTMUX

			hDlg = TNC->hDlg;

#ifndef LINBPQ

			if (hDlg == 0)
			{
				// Running on a port without a window, eg  UZ7HO or MultiPSK

				CreatePactorWindow(TNC, "RIGCONTROL", "RigControl", 10, PacWndProc, 350, 80);
				hDlg = TNC->hDlg;
				TNC->ClientHeight = 80;
				TNC->ClientWidth = 350;
			}

			RigRow = TNC->RigControlRow;

			RIG->hLabel = CreateWindow(WC_STATIC , "", WS_CHILD | WS_VISIBLE,
				10, RigRow, 80,20, hDlg, NULL, hInstance, NULL);
	
			RIG->hCAT = CreateWindow(WC_STATIC , "",  WS_CHILD | WS_VISIBLE,
                 90, RigRow, 40,20, hDlg, NULL, hInstance, NULL);
	
			RIG->hFREQ = CreateWindow(WC_STATIC , "",  WS_CHILD | WS_VISIBLE,
                 135, RigRow, 100,20, hDlg, NULL, hInstance, NULL);
	
			RIG->hMODE = CreateWindow(WC_STATIC , "",  WS_CHILD | WS_VISIBLE,
                 240, RigRow, 60,20, hDlg, NULL, hInstance, NULL);
	
			RIG->hSCAN = CreateWindow(WC_STATIC , "",  WS_CHILD | WS_VISIBLE,
                 300, RigRow, 20,20, hDlg, NULL, hInstance, NULL);

			RIG->hPTT = CreateWindow(WC_STATIC , "",  WS_CHILD | WS_VISIBLE,
                 320, RigRow, 20,20, hDlg, NULL, hInstance, NULL);

		//if (PORT->PortType == ICOM)
		//{
		//	sprintf(msg,"%02X", PORT->Rigs[i].RigAddr);
		//	SetWindowText(RIG->hCAT, msg);
		//}
			SetWindowText(RIG->hLabel, RIG->RigName);
#endif

		RIG->WEB_Label = _strdup(RIG->RigName);
//		RIG->WEB_CAT;
		RIG->WEB_FREQ = zalloc(80);
		RIG->WEB_MODE = zalloc(80);
		strcpy(RIG->WEB_FREQ, "-----------");
		strcpy(RIG->WEB_MODE, "------");

		RIG->WEB_PTT = ' ';
		RIG->WEB_SCAN = ' ';
		}
	}

   
	if (NumberofPorts == 0)
	{
		SetupPortRIGPointers();
		return TRUE;
	}

	Row = 0;

	for (p = 0; p < NumberofPorts; p++)
	{
		PORT = PORTInfo[p];
		
//		CreateDisplay(PORT);

		if (PORT->PTC == 0)		// Not using Rig Port on a PTC
			OpenRigCOMMPort(PORT, PORT->IOBASE, PORT->SPEED);

		if (PORT->PTTIOBASE[0])		// Using separare port for PTT?
		{
			if (PORT->PTTIOBASE[3] == '=')
				PORT->hPTTDevice = OpenCOMPort(&PORT->PTTIOBASE[4], PORT->SPEED, FALSE, FALSE, FALSE, 0);
			else
				PORT->hPTTDevice = OpenCOMPort(&PORT->PTTIOBASE[3], PORT->SPEED, FALSE, FALSE, FALSE, 0);
		}
		else
			PORT->hPTTDevice = PORT->hDevice;	// Use same port for PTT
	}

	for (p = 0; p < NumberofPorts; p++)
	{
		PORT = PORTInfo[p];

		for (i=0; i < PORT->ConfiguredRigs; i++)
		{
			int j;
			int k = 0;
			int BitMask;
			struct _EXTPORTDATA * PortEntry;

			RIG = &PORT->Rigs[i];

			SetupScanInterLockGroups(RIG);

			// Get record for each port in Port Bitmap

			// The Scan "Request Permission to Change" code needs the Port Records in order - 
			// Those with active connect lock (eg SCS) first, then those with just a connect pending lock (eg WINMOR)
			// then those with neither
			
			BitMask = RIG->BPQPort;
			for (j = 0; j < 32; j++)
			{
				if (BitMask & 1)
				{
					PortEntry = (struct _EXTPORTDATA *)GetPortTableEntryFromPortNum(j);		// BPQ32 port record for this port
					if (PortEntry)
						if (PortEntry->SCANCAPABILITIES == CONLOCK)
							RIG->PortRecord[k++] = PortEntry;
				}
				BitMask >>= 1;
			}

			BitMask = RIG->BPQPort;
			for (j = 0; j < 32; j++)
			{
				if (BitMask & 1)
				{
					PortEntry = (struct _EXTPORTDATA *)GetPortTableEntryFromPortNum(j);		// BPQ32 port record for this port
					if (PortEntry)
						if (PortEntry->SCANCAPABILITIES == SIMPLE)
							RIG->PortRecord[k++] = PortEntry;
				}
				BitMask >>= 1;
			}

			BitMask = RIG->BPQPort;
			for (j = 0; j < 32; j++)
			{
				if (BitMask & 1)
				{
					PortEntry = (struct _EXTPORTDATA *)GetPortTableEntryFromPortNum(j);		// BPQ32 port record for this port
					if (PortEntry)
						if (PortEntry->SCANCAPABILITIES == NONE)
							RIG->PortRecord[k++] = PortEntry;
				}
				BitMask >>= 1;
			}

			RIG->PORT = PORT;		// For PTT
			
			if (RIG->NumberofBands)
				CheckTimeBands(RIG);		// Set initial timeband

#ifdef WIN32
			if (RIG->PTTCATPort[0])			// Serial port RTS to CAT 
				_beginthread(PTTCATThread,0,RIG);
#endif
		}
	}
//	MoveWindow(hDlg, Rect.left, Rect.top, Rect.right - Rect.left, Row + 100, TRUE);

	SetupPortRIGPointers();

	Debugprintf("PORTTYPE %d", PORTInfo[0]->PortType);

	WritetoConsole("\nRig Control Enabled\n");

	return TRUE;
}

DllExport BOOL APIENTRY Rig_Close()
{
	struct RIGPORTINFO * PORT;
	struct TNCINFO * TNC;
	int n, p;

	for (p = 0; p < NumberofPorts; p++)
	{
		PORT = PORTInfo[p];

		if (PORT->PortType == NMEA)
		{
			// Send Remote OFF

			int i;
			char REMOFF[80];

			i = sprintf(REMOFF, "$PICOA,90,%02x,REMOTE,OFF*xx\r\n", PORT->Rigs[0].RigAddr);
			AddNMEAChecksum(REMOFF);
	
			WriteCOMBlock(PORT->hDevice, REMOFF, i);
			Sleep(200);
		}

		CloseCOMPort(PORT->hDevice);

		
		if (PORT->hPTTDevice != PORT->hDevice)
			CloseCOMPort(PORT->hPTTDevice);

		PORT->hDevice = 0;
		PORT->hPTTDevice = 0;

		// Free the RIG and Port Records

		for (n = 0; n < PORT->ConfiguredRigs; n++)
		{
			struct RIGINFO * RIG = &PORT->Rigs[n];
			
			if (RIG->TimeBands)
				free (RIG->TimeBands[1]->Scanlist);

			if (RIG->PTTCATPort[0])
			{
				Rig_PTT(RIG, FALSE);				// Make sure PTT is down
				EndPTTCATThread = TRUE;
			}
		}


		free (PORT);
		PORTInfo[p] = NULL;
	}

	NumberofPorts = 0;		// For possible restart

	// And free the TNC config info

	for (p = 1; p < 33; p++)
	{
		TNC = TNCInfo[p];

		if (TNC == NULL)
			continue;

		TNC->RIG = NULL;

//		memset(TNC->WL2KInfoList, 0, sizeof(TNC->WL2KInfoList));

	}

	return TRUE;
}

BOOL Rig_Poll()
{
	int p, i;
	
	struct RIGPORTINFO * PORT;
	struct RIGINFO * RIG;

	for (p = 0; p < NumberofPorts; p++)
	{
		PORT = PORTInfo[p];

		if (PORT->PortType == DUMMY)
		{
			DummyPoll(PORT);
			return TRUE;
		}

		if (PORT->hDevice == 0)			// Try to reopen
		{
			// Try to reopen every 15 secs 
			
			PORT->ReopenDelay++;

			if (PORT->ReopenDelay > 150)
			{
				PORT->ReopenDelay = 0;
				OpenRigCOMMPort(PORT, PORT->IOBASE, PORT->SPEED);
			}
		}
		if (PORT == NULL || (PORT->hDevice == 0 && PORT->PTC == 0))
			continue;

		// Check PTT Timers

		for (i=0; i< PORT->ConfiguredRigs; i++)
		{
			RIG = &PORT->Rigs[i];

			if (RIG->PTTTimer)
			{
				RIG->PTTTimer--;
				if (RIG->PTTTimer == 0)
					Rig_PTT(RIG, FALSE);
			}
		}

		CheckRX(PORT);

		switch (PORT->PortType)
		{ 
		case ICOM:
			
			ICOMPoll(PORT);
			break;

		case YAESU:
		case FT100:
		case FT990:
		case FT1000:
			
			YaesuPoll(PORT);
			break;

		case KENWOOD:
		case FT2000:
		case FLEX:
		case NMEA:
			
			KenwoodPoll(PORT);
			break;
		}
	}

	return TRUE;
}
 

BOOL RigCloseConnection(struct RIGPORTINFO * PORT)
{
   // disable event notification and wait for thread
   // to halt

   CloseCOMPort(PORT->hDevice); 
   return TRUE;

} // end of CloseConnection()

#ifndef WIN32
#define ONESTOPBIT          0
#define ONE5STOPBITS        1
#define TWOSTOPBITS         2
#endif

OpenRigCOMMPort(struct RIGPORTINFO * PORT, VOID * Port, int Speed)
{
	if (PORT->PortType == FT2000 || strcmp(PORT->Rigs[0].RigName, "FT847") == 0)		// FT2000 and similar seem to need two stop bits
		PORT->hDevice = OpenCOMPort((VOID *)Port, Speed, FALSE, FALSE, FALSE, TWOSTOPBITS);
	else if (PORT->PortType == NMEA)
		PORT->hDevice = OpenCOMPort((VOID *)Port, Speed, FALSE, FALSE, FALSE, ONESTOPBIT);
	else
		PORT->hDevice = OpenCOMPort((VOID *)Port, Speed, FALSE, FALSE, FALSE, TWOSTOPBITS);

	if (PORT->hDevice == 0)
		return (FALSE);

	if (PORT->PortType != PTT)
	{
		COMSetRTS(PORT->hDevice);
		COMSetDTR(PORT->hDevice);
	}

	if (strcmp(PORT->Rigs[0].RigName, "FT847") == 0)
	{
		// Looks like FT847 Needa a "Cat On" Command

		UCHAR CATON[6] = {0,0,0,0,0};

		WriteCOMBlock(PORT->hDevice, CATON, 5);
	}

	if (PORT->PortType == NMEA)
	{
		// Looks like NMEA Needs Remote ON

		int i;
		char REMON[80];

		i = sprintf(REMON, "$PICOA,90,%02x,REMOTE,ON*xx\r\n", PORT->Rigs[0].RigAddr);
		AddNMEAChecksum(REMON);

		WriteCOMBlock(PORT->hDevice, REMON, i);
	}


	return TRUE;
}

void CheckRX(struct RIGPORTINFO * PORT)
{
	int Length;
	char NMEAMsg[100];
	unsigned char * ptr;
	int len;

	if (PORT->PTC)
	{
		Length = GetPTCRadioCommand(PORT->PTC, &PORT->RXBuffer[PORT->RXLen]);
	}
	else
	{
		if (PORT->hDevice == 0) 
			return;

		// only try to read number of bytes in queue 

		if (PORT->RXLen == 500)
			PORT->RXLen = 0;

		Length = 500 - (DWORD)PORT->RXLen;

		Length = ReadCOMBlock(PORT->hDevice, &PORT->RXBuffer[PORT->RXLen], Length);
	}

	if (Length == 0)
		return;					// Nothing doing
	
	PORT->RXLen += Length;

	Length = PORT->RXLen;

	switch (PORT->PortType)
	{ 
	case ICOM:
	
		if (Length < 6)				// Minimum Frame Sise
			return;

		if (PORT->RXBuffer[Length-1] != 0xfd)
			return;	

		ProcessICOMFrame(PORT, PORT->RXBuffer, Length);	// Could have multiple packets in buffer

		PORT->RXLen = 0;		// Ready for next frame	
		return;
	
	case YAESU:

		// Possible responses are a single byte ACK/NAK or a 5 byte info frame

		if (Length == 1 && PORT->CmdSent > 0)
		{
			ProcessYaesuCmdAck(PORT);
			return;
		}
	
		if (Length < 5)			// Frame Sise
			return;

		if (Length > 5)			// Frame Sise
		{
			PORT->RXLen = 0;	// Corruption - reset and wait for retry	
			return;
		}

		ProcessYaesuFrame(PORT);

		PORT->RXLen = 0;		// Ready for next frame	
		return;

	case FT100:

		// Only response should be a 16 byte info frame

		if (Length < 32)		// Frame Sise  why???????
			return;

		if (Length > 32)			// Frame Sise
		{
			PORT->RXLen = 0;	// Corruption - reset and wait for retry	
			return;
		}

		ProcessFT100Frame(PORT);

		PORT->RXLen = 0;		// Ready for next frame	
		return;

	case FT990:

		// Only response should be a 32 byte info frame
	
		if (Length < 32)			// Frame Sise
			return;

		if (Length > 32)			// Frame Sise
		{
			PORT->RXLen = 0;		// Corruption - reset and wait for retry	
			return;
		}

		ProcessFT990Frame(PORT);
		PORT->RXLen = 0;		// Ready for next frame	
		return;


	case FT1000:

		// Only response should be a 16 byte info frame
	
		ptr = PORT->RXBuffer;

		if (Length < 16)			// Frame Sise
			return;

		if (Length > 16)			// Frame Sise
		{
			PORT->RXLen = 0;		// Corruption - reset and wait for retry	
			return;
		}

		ProcessFT1000Frame(PORT);

		PORT->RXLen = 0;		// Ready for next frame	
		return;

	case KENWOOD:
	case FT2000:
	case FLEX:

		if (Length < 2)				// Minimum Frame Sise
			return;

		if (Length > 50)			// Garbage
		{
			PORT->RXLen = 0;		// Ready for next frame	
			return;
		}

		if (PORT->RXBuffer[Length-1] != ';')
			return;	

		ProcessKenwoodFrame(PORT, Length);	

		PORT->RXLen = 0;		// Ready for next frame	
		return;
	
	case NMEA:

		ptr = memchr(PORT->RXBuffer, 0x0a, Length);

		while (ptr != NULL)
		{
			ptr++;									// include lf
			len = ptr - &PORT->RXBuffer[0];	
			
			memcpy(NMEAMsg, PORT->RXBuffer, len);	

			NMEAMsg[len] = 0;

//			if (Check0183CheckSum(NMEAMsg, len))
				ProcessNMEA(PORT, NMEAMsg, len);

			Length -= len;							// bytes left

			if (Length > 0)
			{
				memmove(PORT->RXBuffer, ptr, Length);
				ptr = memchr(PORT->RXBuffer, 0x0a, Length);
			}
			else
				ptr=0;
		}

		PORT->RXLen = Length;
	}
}

VOID ProcessICOMFrame(struct RIGPORTINFO * PORT, UCHAR * rxbuffer, int Len)
{
	UCHAR * FendPtr;
	int NewLen;

	//	Split into Packets. By far the most likely is a single KISS frame
	//	so treat as special case
	
	FendPtr = memchr(rxbuffer, 0xfd, Len);
	
	if (FendPtr == &rxbuffer[Len-1])
	{
		ProcessFrame(PORT, rxbuffer, Len);
		return;
	}
		
	// Process the first Packet in the buffer

	NewLen =  FendPtr - rxbuffer +1;

	ProcessFrame(PORT, rxbuffer, NewLen);
	
	// Loop Back

	ProcessICOMFrame(PORT, FendPtr+1, Len - NewLen);
	return;
}



BOOL RigWriteCommBlock(struct RIGPORTINFO * PORT)
{
	// if using a PTC radio interface send to the SCSPactor Driver, else send to COM port

	if (PORT->PTC)
		SendPTCRadioCommand(PORT->PTC, PORT->TXBuffer, PORT->TXLen);
	else
	{
		BOOL        fWriteStat;
		DWORD       BytesWritten;
		DWORD       ErrorFlags;

#ifdef LINBPQ
		BytesWritten = write(PORT->hDevice, PORT->TXBuffer, PORT->TXLen);
#else
		fWriteStat = WriteFile(PORT->hDevice, PORT->TXBuffer, PORT->TXLen, &BytesWritten, NULL );
#endif
		if (PORT->TXLen != BytesWritten)
		{
			if (PORT->hDevice)
				 CloseCOMPort(PORT->hDevice);
			OpenRigCOMMPort(PORT, PORT->IOBASE, PORT->SPEED);
			if (PORT->hDevice)
			{
				// Try Again

#ifdef LINBPQ
				BytesWritten = write(PORT->hDevice, PORT->TXBuffer, PORT->TXLen);
#else
				fWriteStat = WriteFile(PORT->hDevice, PORT->TXBuffer, PORT->TXLen, &BytesWritten, NULL );
#endif
			}
		}
	}

	PORT->Timeout = 100;		// 2 secs
	return TRUE;  
}

VOID ReleasePermission(struct RIGINFO *RIG)
{
	int i = 0;
	struct _EXTPORTDATA * PortRecord;

	while (RIG->PortRecord[i])
	{
		PortRecord = RIG->PortRecord[i];
		PortRecord->PORT_EXT_ADDR(6, PortRecord->PORTCONTROL.PORTNUMBER, 3);	// Release Perrmission
		i++;
	}
}

GetPermissionToChange(struct RIGPORTINFO * PORT, struct RIGINFO *RIG)
{
	struct ScanEntry ** ptr;
	struct _EXTPORTDATA * PortRecord;
	int i;

	// Get Permission to change

	if (RIG->WaitingForPermission)
	{
		// TNC has been asked for permission, and we are waiting respoonse
		
		RIG->OKtoChange = RIG->PortRecord[0]->PORT_EXT_ADDR(6, RIG->PortRecord[0]->PORTCONTROL.PORTNUMBER, 2);	// Get Ok Flag
	
		if (RIG->OKtoChange == 1)
			goto DoChange;

		if (RIG->OKtoChange == -1)
		{
			// Permission Refused. Wait Scan Interval and try again

			Debugprintf("Scan Debug %s Refused permission - waiting ScanInterval %d",
				RIG->PortRecord[0]->PORT_DLL_NAME, PORT->FreqPtr->Dwell ); 

			RIG->WaitingForPermission = FALSE;
			SetWindowText(RIG->hSCAN, "-");
			RIG->WEB_SCAN = '=';

			RIG->ScanCounter = PORT->FreqPtr->Dwell; 
			
			if (RIG->ScanCounter == 0 || RIG->ScanCounter > 150)		// ? After manual change
				RIG->ScanCounter = 50;

			return FALSE;
		}
		
		return FALSE;			// Haven't got reply yet.
	}
	else
	{
		if (RIG->PortRecord[0]->PORT_EXT_ADDR)
			RIG->WaitingForPermission = RIG->PortRecord[0]->PORT_EXT_ADDR(6, RIG->PortRecord[0]->PORTCONTROL.PORTNUMBER, 1);	// Request Perrmission
				
		// If it returns zero there is no need to wait.
				
		if (RIG->WaitingForPermission)
		{
			return FALSE;
		}		
	}

DoChange:

	// First TNC has given permission. Ask any others (these are assumed to give immediate yes/no

	i = 1;

	while (RIG->PortRecord[i])
	{
		PortRecord = RIG->PortRecord[i];

		if (PortRecord->PORT_EXT_ADDR(6, PortRecord->PORTCONTROL.PORTNUMBER, 1))
		{
			// 1 means can't change - release all

			Debugprintf("Scan Debug %s Refused permission - waiting ScanInterval %d",
				PortRecord->PORT_DLL_NAME, PORT->FreqPtr->Dwell); 

			RIG->WaitingForPermission = FALSE;
			SetWindowText(RIG->hSCAN, "-");
			RIG->WEB_SCAN = '-';
			RIG->ScanCounter = PORT->FreqPtr->Dwell;

			if (RIG->ScanCounter == 0 || RIG->ScanCounter > 150)		// ? After manual change
				RIG->ScanCounter = 50; 

			ReleasePermission(RIG);
			return FALSE;
		}
		i++;
	}


	RIG->WaitingForPermission = FALSE;

	// Update pointer to next frequency

	RIG->FreqPtr++;

	ptr = RIG->FreqPtr;

	if (ptr == NULL)
	{
		Debugprintf("Scan Debug - No freqs - quitting"); 
		return FALSE;					 // No Freqs
	}

	if (ptr[0] == (struct ScanEntry *)0) // End of list - reset to start
	{
		ptr = CheckTimeBands(RIG);
	}

	PORT->FreqPtr = ptr[0];				// Save Scan Command Block

	RIG->ScanCounter = PORT->FreqPtr->Dwell; 
	
	SetWindowText(RIG->hSCAN, "S");
	RIG->WEB_SCAN = 'S';

	// Do Bandwidth and antenna switches (if needed)

	DoBandwidthandAntenna(RIG, ptr[0]);

	return TRUE;
}

VOID DoBandwidthandAntenna(struct RIGINFO *RIG, struct ScanEntry * ptr)
{
	// If Bandwidth Change needed, do it

	int i;
	struct _EXTPORTDATA * PortRecord;

	if (ptr->Bandwidth || ptr->RPacketMode || ptr->HFPacketMode || ptr->PMaxLevel)
	{
		i = 0;

		while (RIG->PortRecord[i])
		{
			PortRecord = RIG->PortRecord[i];

			RIG->CurrentBandWidth = ptr->Bandwidth;

			PortRecord->PORT_EXT_ADDR(6, PortRecord->PORTCONTROL.PORTNUMBER, ptr);

/*			if (ptr->Bandwidth == 'R')			// Robust Packet
				PortRecord->PORT_EXT_ADDR(6, PortRecord->PORTCONTROL.PORTNUMBER, 6);	// Set Robust Packet
			else 
				
			if (ptr->Bandwidth == 'W')
				PortRecord->PORT_EXT_ADDR(6, PortRecord->PORTCONTROL.PORTNUMBER, 4);	// Set Wide Mode
			else
				PortRecord->PORT_EXT_ADDR(6, PortRecord->PORTCONTROL.PORTNUMBER, 5);	// Set Narrow Mode
*/
			i++;
		}
	}

	// If Antenna Change needed, do it

	if (ptr->Antenna)
	{
		SwitchAntenna(RIG, ptr->Antenna);
	}

	return;	
}

VOID ICOMPoll(struct RIGPORTINFO * PORT)
{
	UCHAR * Poll = PORT->TXBuffer;
	int i;

	struct RIGINFO * RIG;

	for (i=0; i< PORT->ConfiguredRigs; i++)
	{
		RIG = &PORT->Rigs[i];

		if (RIG->ScanStopped == 0)
			if (RIG->ScanCounter)
				RIG->ScanCounter--;
	}

	if (PORT->Timeout)
	{
		PORT->Timeout--;
		
		if (PORT->Timeout)			// Still waiting
			return;

		PORT->Retries--;

		if(PORT->Retries)
		{
			RigWriteCommBlock(PORT);	// Retransmit Block
			return;
		}

		RIG = &PORT->Rigs[PORT->CurrentRig];


		SetWindowText(RIG->hFREQ, "-----------");
		strcpy(RIG->WEB_FREQ, "-----------");
		SetWindowText(RIG->hMODE, "------");
		strcpy(RIG->WEB_MODE, "------");

//		SetWindowText(RIG->hFREQ, "145.810000");
//		SetWindowText(RIG->hMODE, "RTTY/1");

		PORT->Rigs[PORT->CurrentRig].RIGOK = FALSE;

		return;

	}

	// Send Data if avail, else send poll

	PORT->CurrentRig++;

	if (PORT->CurrentRig >= PORT->ConfiguredRigs)
		PORT->CurrentRig = 0;

	RIG = &PORT->Rigs[PORT->CurrentRig];

	RIG->DebugDelay ++;

/*
	if (RIG->DebugDelay > 600)
	{
		RIG->DebugDelay = 0;
		Debugprintf("Scan Debug %d %d %d %d %d %d", PORT->CurrentRig, 
			RIG->NumberofBands, RIG->RIGOK, RIG->ScanStopped, RIG->ScanCounter,
			RIG->WaitingForPermission);
	}
*/
	if (RIG->NumberofBands && RIG->RIGOK && (RIG->ScanStopped == 0))
	{
		if (RIG->ScanCounter <= 0)
		{
			//	Send Next Freq

			if	(GetPermissionToChange(PORT, RIG))
			{
				if (RIG_DEBUG)
					Debugprintf("BPQ32 Change Freq to %9.4f", PORT->FreqPtr->Freq/1000000.0);

				memcpy(PORT->TXBuffer, PORT->FreqPtr->Cmd1, PORT->FreqPtr->Cmd1Len);
	
				PORT->TXLen = PORT->FreqPtr->Cmd1Len;
				RigWriteCommBlock(PORT);
				PORT->Retries = 2;
				PORT->AutoPoll = TRUE;

				return;
			}
		}
	}

	if (RIG->RIGOK && RIG->BPQtoRADIO_Q)
	{
		int datalen;
		UINT * buffptr;
			
		buffptr=Q_REM(&RIG->BPQtoRADIO_Q);

		datalen=buffptr[1];

		// Copy the ScanEntry struct from the Buffer to the PORT Scanentry

		memcpy(&PORT->ScanEntry, buffptr+2, sizeof(struct ScanEntry));

		PORT->FreqPtr = &PORT->ScanEntry;		// Block we are currently sending.

		if (RIG_DEBUG)
			Debugprintf("BPQ32 Manual Change Freq to %9.4f", PORT->FreqPtr->Freq/1000000.0);


		memcpy(Poll, &buffptr[20], PORT->FreqPtr->Cmd1Len);

		if (PORT->ScanEntry.Cmd2)
		{
			PORT->ScanEntry.Cmd2 = (char *)&PORT->Line2;	// Put The Set mode Command into ScanStruct
			memcpy(PORT->Line2, &buffptr[30], PORT->FreqPtr->Cmd2Len);

			if (PORT->ScanEntry.Cmd3)
			{
				PORT->ScanEntry.Cmd3 = (char *)&PORT->Line3;
				memcpy(PORT->Line3, &buffptr[40], PORT->FreqPtr->Cmd3Len);
			}
		}

		DoBandwidthandAntenna(RIG, &PORT->ScanEntry);

		
		PORT->TXLen = PORT->FreqPtr->Cmd1Len;					// First send the set Freq
		RigWriteCommBlock(PORT);
		PORT->Retries = 2;

		ReleaseBuffer(buffptr);

		PORT->AutoPoll = FALSE;

		return;
	}

	if (RIG->RIGOK && (RIG->ScanStopped == 0) && RIG->NumberofBands)
		return;						// no point in reading freq if we are about to change it
		
	if (RIG->PollCounter)
	{
		RIG->PollCounter--;
		if (RIG->PollCounter)
			return;
	}

	if (RIG->RIGOK)
	{
		PORT->Retries = 2;
		RIG->PollCounter = 10 / PORT->ConfiguredRigs;			// Once Per Sec
	}
	else
	{
		PORT->Retries = 1;
		RIG->PollCounter = 100 / PORT->ConfiguredRigs;			// Slow Poll if down
	}

	PORT->AutoPoll = TRUE;

	// Read Frequency 

	Poll[0] = 0xFE;
	Poll[1] = 0xFE;
	Poll[2] = RIG->RigAddr;
	Poll[3] = 0xE0;
	Poll[4] = 0x3;		// Get frequency command
	Poll[5] = 0xFD;

	PORT->TXLen = 6;

	RigWriteCommBlock(PORT);
	return;
}


VOID ProcessFrame(struct RIGPORTINFO * PORT, UCHAR * Msg, int framelen)
{
	UCHAR * Poll = PORT->TXBuffer;
	struct RIGINFO * RIG;
	int i;

	if (Msg[0] != 0xfe || Msg[1] != 0xfe)

		// Duff Packer - return

		return;	

	if (Msg[2] != 0xe0)
	{
		// Echo - Proves a CI-V interface is attached

		if (PORT->PORTOK == FALSE)
		{
			// Just come up		
			PORT->PORTOK = TRUE;
		}
		return;
	}

	for (i=0; i< PORT->ConfiguredRigs; i++)
	{
		RIG = &PORT->Rigs[i];
		if (Msg[3] == RIG->RigAddr)
			goto ok;
	}

	return;

ok:

	if (Msg[4] == 0xFB)
	{
		// Accept

		// if it was the set freq, send the set mode

		if (PORT->TXBuffer[4] == 5 || PORT->TXBuffer[4] == 7) // Freq or VFO
		{
			if (PORT->FreqPtr->Cmd2)
			{
				memcpy(PORT->TXBuffer, PORT->FreqPtr->Cmd2, PORT->FreqPtr->Cmd2Len);
				PORT->TXLen = PORT->FreqPtr->Cmd2Len;
				RigWriteCommBlock(PORT);
				PORT->Retries = 2;
			}
			else
			{
				if (!PORT->AutoPoll)
					SendResponse(RIG->Session, "Frequency Set OK");
		
				PORT->Timeout = 0;

			}

			return;
		}

		if (PORT->TXBuffer[4] == 6)
		{
			if (PORT->FreqPtr->Cmd3)
			{
				memcpy(PORT->TXBuffer, PORT->FreqPtr->Cmd3, PORT->FreqPtr->Cmd3Len);
				PORT->TXLen = PORT->FreqPtr->Cmd3Len;
				RigWriteCommBlock(PORT);
				PORT->Retries = 2;
				return;
			}

			goto SetFinished;
		}

		if (PORT->TXBuffer[4] == 0x0f || PORT->TXBuffer[4] == 0x01a)	// Set DUP or Set Data
			goto SetFinished;

		if (PORT->TXBuffer[4] == 0x08)
		{
			// Memory Chan
			
			PORT->TXBuffer[4] = 0;			// So we only do it once

			goto SetFinished;
		}

		if (PORT->TXBuffer[4] == 0x08 || PORT->TXBuffer[4] == 0x0f || PORT->TXBuffer[4] == 0x01a)	// Set DUP or Set Data
		{

SetFinished:

			// Set Mode Response - if scanning read freq, else return OK to user

			if (RIG->ScanStopped == 0)
			{
				ReleasePermission(RIG);	// Release Perrmission

				Poll[0] = 0xFE;
				Poll[1] = 0xFE;
				Poll[2] = RIG->RigAddr;
				Poll[3] = 0xE0;
				Poll[4] = 0x3;		// Get frequency command
				Poll[5] = 0xFD;

				PORT->TXLen = 6;
				RigWriteCommBlock(PORT);
				PORT->Retries = 2;
				return;
			}

			else
				if (!PORT->AutoPoll)
					SendResponse(RIG->Session, "Frequency and Mode Set OK");
		}

		PORT->Timeout = 0;
		return;
	}

	if (Msg[4] == 0xFA)
	{
		// Reject

		PORT->Timeout = 0;

		if (!PORT->AutoPoll)
		{
			if (PORT->TXBuffer[4] == 5)
				SendResponse(RIG->Session, "Sorry - Set Frequency Command Rejected");
			else
			if (PORT->TXBuffer[4] == 6)
				SendResponse(RIG->Session, "Sorry - Set Mode Command Rejected");
			else
			if (PORT->TXBuffer[4] == 0x0f)
				SendResponse(RIG->Session, "Sorry - Set Shift Command Rejected");
		}
		return;
	}

	if (Msg[4] == PORT->TXBuffer[4])
	{
		// Response to our command

		// Any valid frame is an ACK

		RIG->RIGOK = TRUE;
		PORT->Timeout = 0;
	}
	else 
		return;		// What does this mean??


	if (PORT->PORTOK == FALSE)
	{
		// Just come up
//		char Status[80];
		
		PORT->PORTOK = TRUE;
//		sprintf(Status,"COM%d PORT link OK", PORT->IOBASE);
//		SetWindowText(PORT->hStatus, Status);
	}

	if (Msg[4] == 3)
	{
		// Rig Frequency
		int n, j, Freq = 0, decdigit;

		for (j = 9; j > 4; j--)
		{
			n = Msg[j];
			decdigit = (n >> 4);
			decdigit *= 10;
			decdigit += n & 0xf;
			Freq = (Freq *100 ) + decdigit;
		}

		RIG->RigFreq = Freq / 1000000.0;

//		Valchar = _fcvt(FreqF, 6, &dec, &sign);
		_gcvt(RIG->RigFreq, 9, RIG->Valchar);
 
		sprintf(RIG->WEB_FREQ,"%s", RIG->Valchar);
		SetWindowText(RIG->hFREQ, RIG->WEB_FREQ);

		// Now get Mode

			Poll[0] = 0xFE;
			Poll[1] = 0xFE;
			Poll[2] = RIG->RigAddr;
			Poll[3] = 0xE0;
			Poll[4] = 0x4;		// Get Mode
			Poll[5] = 0xFD;

		PORT->TXLen = 6;
		RigWriteCommBlock(PORT);
		PORT->Retries = 2;
		return;
	}
	if (Msg[4] == 4)
	{
		// Mode

		unsigned int Mode;
		
		Mode = (Msg[5] >> 4);
		Mode *= 10;
		Mode += Msg[5] & 0xf;

		if (Mode > 17) Mode = 17;

		sprintf(RIG->WEB_MODE,"%s/%d", Modes[Mode], Msg[6]);
		SetWindowText(RIG->hMODE, RIG->WEB_MODE);
	}
}

SendResponse(int Session, char * Msg)
{
	PMESSAGE Buffer = GetBuff();
	BPQVECSTRUC * VEC;
	TRANSPORTENTRY * L4 = L4TABLE;

	if (Session == -1)
		return 0;

	L4 += Session;

	Buffer->LENGTH = sprintf((char *)Buffer, "       \xf0%s\r", Msg);

	VEC = L4->L4TARGET.HOST;

	C_Q_ADD(&L4->L4TX_Q, (UINT *)Buffer);

#ifndef LINBPQ

	if (VEC)
		PostMessage(VEC->HOSTHANDLE, BPQMsg, VEC->HOSTSTREAM, 2);  
#endif
	return 0;
}

VOID ProcessFT100Frame(struct RIGPORTINFO * PORT)
{
	// Only one we should see is a Status Message

	UCHAR * Poll = PORT->TXBuffer;
	UCHAR * Msg = PORT->RXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu
	int Freq;
	double FreqF;
	unsigned int Mode;
	
	RIG->RIGOK = TRUE;
	PORT->Timeout = 0;

	// Bytes 0 is Band
	// 1 - 4 is Freq in binary in units of 1.25 HZ (!)
	// Byte 5 is Mode

	Freq =  (Msg[1] << 24) + (Msg[2] << 16) + (Msg[3] << 8) + Msg[4];
	
	FreqF = (Freq * 1.25) / 1000000;

	if (PORT->YaesuVariant == FT1000MP)
		FreqF = FreqF / 2;				// No idea why!

	_gcvt(FreqF, 9, RIG->Valchar);

	sprintf(RIG->WEB_FREQ,"%s", RIG->Valchar);
	SetWindowText(RIG->hFREQ, RIG->WEB_FREQ);

	if (PORT->PortType == FT100)
	{
		Mode = Msg[5] & 15;
		if (Mode > 8) Mode = 8;
		sprintf(RIG->WEB_MODE,"%s", FT100Modes[Mode]);
	}
	else	// FT1000
	{
		Mode = Msg[7] & 7;
		sprintf(RIG->WEB_MODE,"%s", FTRXModes[Mode]);
	}

	SetWindowText(RIG->hMODE, RIG->WEB_MODE);

	if (!PORT->AutoPoll)
		SendResponse(RIG->Session, "Mode and Frequency Set OK");
	else
		if (PORT->TXLen > 5)		// Poll is 5 Change is more
			ReleasePermission(RIG);		// Release Perrmission to change
}



VOID ProcessFT990Frame(struct RIGPORTINFO * PORT)
{
	// Only one we should see is a Status Message

	UCHAR * Poll = PORT->TXBuffer;
	UCHAR * Msg = PORT->RXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu
	int Freq;
	double FreqF;
	unsigned int Mode;
	
	RIG->RIGOK = TRUE;
	PORT->Timeout = 0;

	// Bytes 0 is Band
	// 1 - 4 is Freq in units of 10Hz (I think!)
	// Byte 5 is Mode

	Freq =  (Msg[1] << 16) + (Msg[2] << 8) + Msg[3];
	
	FreqF = (Freq * 10.0) / 1000000;

	_gcvt(FreqF, 9, RIG->Valchar);

	sprintf(RIG->WEB_FREQ,"%s", RIG->Valchar);
	SetWindowText(RIG->hFREQ, RIG->WEB_FREQ);

	Mode = Msg[7] & 7;
	sprintf(RIG->WEB_MODE,"%s", FTRXModes[Mode]);

	SetWindowText(RIG->hMODE, RIG->WEB_MODE);

	if (!PORT->AutoPoll)
		SendResponse(RIG->Session, "Mode and Frequency Set OK");
	else
		if (PORT->TXLen > 5)		// Poll is 5 change is more
			ReleasePermission(RIG);		// Release Perrmission to change
}

VOID ProcessFT1000Frame(struct RIGPORTINFO * PORT)
{
	// Only one we should see is a Status Message

	UCHAR * Poll = PORT->TXBuffer;
	UCHAR * Msg = PORT->RXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu
	int Freq;
	double FreqF;
	unsigned int Mode;
	
	RIG->RIGOK = TRUE;
	PORT->Timeout = 0;

	// I think the FT1000/1000D is same as 990
	//	FT1000MP is similar to FT100, but steps on .625 Hz (despite manual)
	// Bytes 0 is Band
	// 1 - 4 is Freq in binary in units of 1.25 HZ (!)
	// Byte 5 is Mode

	if (PORT->YaesuVariant == FT1000MP)
	{
		Freq =  (Msg[1] << 24) + (Msg[2] << 16) + (Msg[3] << 8) + Msg[4];	
		FreqF = (Freq * 1.25) / 1000000;
		FreqF = FreqF / 2;				// No idea why!
	}
	else
	{
		Freq =  (Msg[1] << 16) + (Msg[2] << 8) + Msg[3];
		FreqF = (Freq * 10.0) / 1000000;
	}

	_gcvt(FreqF, 9, RIG->Valchar);

	sprintf(RIG->WEB_FREQ,"%s", RIG->Valchar);
	SetWindowText(RIG->hFREQ, RIG->WEB_FREQ);

	if (PORT->PortType == FT100)
	{
		Mode = Msg[5] & 15;
		if (Mode > 8) Mode = 8;
		sprintf(RIG->WEB_MODE,"%s", FT100Modes[Mode]);
	}
	else	// FT1000
	{
		Mode = Msg[7] & 7;
		sprintf(RIG->WEB_MODE,"%s", FTRXModes[Mode]);
	}

	SetWindowText(RIG->hMODE, RIG->WEB_MODE);

	if (!PORT->AutoPoll)
		SendResponse(RIG->Session, "Mode and Frequency Set OK");
	else
		if (PORT->TXLen > 5)		// Poll is 5 change is more
			ReleasePermission(RIG);		// Release Perrmission to change
}





VOID ProcessYaesuCmdAck(struct RIGPORTINFO * PORT)
{
	UCHAR * Poll = PORT->TXBuffer;
	UCHAR * Msg = PORT->RXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu

	PORT->Timeout = 0;
	PORT->RXLen = 0;					// Ready for next frame	

	if (PORT->CmdSent == 1)				// Set Freq
	{
		ReleasePermission(RIG);			// Release Perrmission

		if (Msg[0])
		{
			// I think nonzero is a Reject

			if (!PORT->AutoPoll)
				SendResponse(RIG->Session, "Sorry - Set Frequency Rejected");

			return;
		}
		else
		{
			if (RIG->ScanStopped == 0)
			{
				// Send a Get Freq - We Don't Poll when scanning

				Poll[0] = Poll[1] = Poll[2] = Poll[3] = 0;
				Poll[4] = 0x3;		// Get frequency amd mode command

				RigWriteCommBlock(PORT);
				PORT->Retries = 2;
				PORT->CmdSent = 0;
			}
			else

			if (!PORT->AutoPoll)
				SendResponse(RIG->Session, "Mode and Frequency Set OK");

			return;
		}
	}

	if (PORT->CmdSent == 7)						// Set Mode
	{
		if (Msg[0])
		{
			// I think nonzero is a Reject

			if (!PORT->AutoPoll)
				SendResponse(RIG->Session, "Sorry - Set Mode Rejected");

			return;
		}
		else
		{
			// Send the Frequency
			
			memcpy(Poll, &Poll[5], 5);
			RigWriteCommBlock(PORT);
			PORT->CmdSent = Poll[4];
			PORT->Retries = 2;

			return;
		}
	}

}
VOID ProcessYaesuFrame(struct RIGPORTINFO * PORT)
{
	UCHAR * Poll = PORT->TXBuffer;
	UCHAR * Msg = PORT->RXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu
	int n, j, Freq = 0, decdigit;
	double FreqF;
	unsigned int Mode;

	// I'm not sure we get anything but a Command Response,
	// and the only command we send is Get Rig Frequency and Mode

	
	RIG->RIGOK = TRUE;
	PORT->Timeout = 0;

	for (j = 0; j < 4; j++)
	{
		n = Msg[j];
		decdigit = (n >> 4);
		decdigit *= 10;
		decdigit += n & 0xf;
		Freq = (Freq *100 ) + decdigit;
	}

	FreqF = Freq / 100000.0;

//		Valchar = _fcvt(FreqF, 6, &dec, &sign);
	_gcvt(FreqF, 9, RIG->Valchar);

	sprintf(RIG->WEB_FREQ,"%s", RIG->Valchar);
	SetWindowText(RIG->hFREQ, RIG->WEB_FREQ);

	Mode = Msg[4];

	if (Mode > 15) Mode = 15;

	sprintf(RIG->WEB_MODE,"%s", YaesuModes[Mode]);
	SetWindowText(RIG->hMODE, RIG->WEB_MODE);

	//	FT847 Manual Freq Change response ends up here
	
	if (strcmp(RIG->RigName, "FT847") == 0)
	{
		if (!PORT->AutoPoll)
			SendResponse(RIG->Session, "Mode and Frequency Set OK");
			
		if (PORT->CmdSent == -1)
			ReleasePermission(RIG);			// Release Perrmission to change
	}
}

VOID YaesuPoll(struct RIGPORTINFO * PORT)
{
	UCHAR * Poll = PORT->TXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu

	if (RIG->ScanStopped == 0)
		if (RIG->ScanCounter)
			RIG->ScanCounter--;

	if (PORT->Timeout)
	{
		PORT->Timeout--;
		
		if (PORT->Timeout)			// Still waiting
			return;

		PORT->Retries--;

		if(PORT->Retries)
		{
			RigWriteCommBlock(PORT);	// Retransmit Block
			return;
		}

		SetWindowText(RIG->hFREQ, "------------------");
		SetWindowText(RIG->hMODE, "----------");
		strcpy(RIG->WEB_FREQ, "-----------");;
		strcpy(RIG->WEB_MODE, "------");


		PORT->Rigs[PORT->CurrentRig].RIGOK = FALSE;

		return;

	}

	// Send Data if avail, else send poll

	if (RIG->NumberofBands && (RIG->ScanStopped == 0))
	{
		if (RIG->ScanCounter <= 0)
		{
			//	Send Next Freq

			if	(GetPermissionToChange(PORT, RIG))
			{
				if (RIG_DEBUG)
					Debugprintf("BPQ32 Change Freq to %9.4f", PORT->FreqPtr->Freq);

				memcpy(PORT->TXBuffer, PORT->FreqPtr->Cmd1, 24);

				if (PORT->PortType == YAESU)
				{
					if (strcmp(PORT->Rigs[0].RigName, "FT847") == 0)
					{
						PORT->TXLen = 15; // No Cmd ACK, so send Mode, Freq and Poll
						PORT->CmdSent = -1;
					}
					else
					{
						PORT->TXLen = 5;
						PORT->CmdSent = Poll[4];
					}
					RigWriteCommBlock(PORT);
					PORT->Retries = 2;
					PORT->AutoPoll = TRUE;
					return;
				}

				// FT100

				PORT->TXLen = 15;			// Set Mode, Set Freq, Poll
				RigWriteCommBlock(PORT);
				PORT->Retries = 2;
				PORT->AutoPoll = TRUE;
			}
		}
	}
	
	if (RIG->RIGOK && RIG->BPQtoRADIO_Q)
	{
		int datalen;
		UINT * buffptr;
			
		buffptr=Q_REM(&RIG->BPQtoRADIO_Q);

		datalen=buffptr[1];

		// Copy the ScanEntry struct from the Buffer to the PORT Scanentry

		memcpy(&PORT->ScanEntry, buffptr+2, sizeof(struct ScanEntry));

		PORT->FreqPtr = &PORT->ScanEntry;		// Block we are currently sending.
		
		if (RIG_DEBUG)
			Debugprintf("BPQ32 Change Freq to %9.4f", PORT->FreqPtr->Freq);

		DoBandwidthandAntenna(RIG, &PORT->ScanEntry);

		memcpy(Poll, &buffptr[20], datalen);

		if (PORT->PortType == YAESU)
		{
			if (strcmp(PORT->Rigs[0].RigName, "FT847") == 0)
			{
				PORT->TXLen = 15;					// Send all
				PORT->CmdSent = -1;
			}
			else
			{
				PORT->TXLen = 5;					// First send the set Freq
				PORT->CmdSent = Poll[4];
			}
		}
		else
			PORT->TXLen = 15;					// Send all

		RigWriteCommBlock(PORT);
		PORT->Retries = 2;

		ReleaseBuffer(buffptr);
		PORT->AutoPoll = FALSE;
	
		return;
	}

	if (RIG->ScanStopped == 0)
		return;						// no point in reading freq if we are about to change it
		
	// Read Frequency 

	Poll[0] = 0;
	Poll[1] = 0;
	Poll[2] = 0;

	if (PORT->PortType == FT990 || PORT->PortType == YAESU || PORT->YaesuVariant == FT1000D)
		Poll[3] = 3;
	else
		Poll[3] = 2;
	
	if (PORT->PortType == YAESU)
		Poll[4] = 0x3;		// Get frequency amd mode command
	else
		Poll[4] = 16;		// FT100/990/1000 Get frequency amd mode command

	PORT->TXLen = 5;
	RigWriteCommBlock(PORT);
	PORT->Retries = 2;
	PORT->CmdSent = 0;

	PORT->AutoPoll = TRUE;

	return;
}

VOID ProcessNMEA(struct RIGPORTINFO * PORT, char * Msg, int Length)
{
	UCHAR * Poll = PORT->TXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu

	Msg[Length] = 0;
	
	if (PORT->PORTOK == FALSE)
	{
		// Just come up		
		PORT->PORTOK = TRUE;
	}

	RIG->RIGOK = TRUE;
	PORT->Timeout = 0;

	if (!PORT->AutoPoll)
	{
		// Response to a RADIO Command

		if (Msg[0] == '?')
			SendResponse(RIG->Session, "Sorry - Command Rejected");
		else
			SendResponse(RIG->Session, "Mode and Frequency Set OK");
	
		PORT->AutoPoll = TRUE;
	}

	if (memcmp(&Msg[13], "RXF", 3) == 0)
	{
		double Freq;

		if (Length < 24)
			return;

		Freq = atof(&Msg[17]);

		sprintf(RIG->WEB_FREQ,"%f", Freq);
		SetWindowText(RIG->hFREQ, RIG->WEB_FREQ);
		strcpy(RIG->Valchar, RIG->WEB_FREQ);

		return;
	}

	if (memcmp(&Msg[13], "MODE", 3) == 0)
	{
		char * ptr;

		if (Length < 24)
			return;

		ptr = strchr(&Msg[18], '*');
		if (ptr) *(ptr) = 0;

		SetWindowText(RIG->hMODE, &Msg[18]);
		strcpy(RIG->WEB_MODE, &Msg[18]);
		return;
	}


}


//FA00014103000;MD2;

VOID ProcessKenwoodFrame(struct RIGPORTINFO * PORT, int Length)
{
	UCHAR * Poll = PORT->TXBuffer;
	UCHAR * Msg = PORT->RXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Yaseu
	UCHAR * ptr;
	int CmdLen;

	Msg[Length] = 0;
	
	if (PORT->PORTOK == FALSE)
	{
		// Just come up		
		PORT->PORTOK = TRUE;
	}

	RIG->RIGOK = TRUE;

	if (!PORT->AutoPoll)
	{
		// Response to a RADIO Command

		if (Msg[0] == '?')
			SendResponse(RIG->Session, "Sorry - Command Rejected");
		else
			SendResponse(RIG->Session, "Mode and Frequency Set OK");
	
		PORT->AutoPoll = TRUE;
		return;
	}


Loop:

	if (PORT->PortType == FLEX)
	{
		Msg += 2;						// Skip ZZ
		Length -= 2;
	}

	ptr = strchr(Msg, ';');
	CmdLen = ptr - Msg +1;

	if (Msg[0] == 'F' && Msg[1] == 'A' && CmdLen > 9)
	{
		char FreqDecimal[10];
		int F1, i;

		if (PORT->PortType == FT2000)
		{
			memcpy(FreqDecimal,&Msg[4], 6);
			Msg[4] = 0;
		}
		else
		{
			memcpy(FreqDecimal,&Msg[7], 6);
			Msg[7] = 0;
		}
		FreqDecimal[6] = 0;

		for (i = 5; i > 2; i--)
		{
			if (FreqDecimal[i] == '0')
				FreqDecimal[i] = 0;
			else
				break;
		}


		F1 = atoi(&Msg[2]);

		sprintf(RIG->WEB_FREQ,"%d.%s", F1, FreqDecimal);
		SetWindowText(RIG->hFREQ, RIG->WEB_FREQ);
		strcpy(RIG->Valchar, RIG->WEB_FREQ);

		PORT->Timeout = 0;
	}
	else if (Msg[0] == 'M' && Msg[1] == 'D')
	{
		int Mode;
		
		if (PORT->PortType == FT2000)
		{
			Mode = Msg[3] - 48;
			if (Mode > 13) Mode = 13;
			SetWindowText(RIG->hMODE, FT2000Modes[Mode]);
			strcpy(RIG->WEB_MODE, FT2000Modes[Mode]);
		}
		else if (PORT->PortType == FLEX)
		{
			Mode = atoi(&Msg[3]);
			if (Mode > 12) Mode = 12;
			SetWindowText(RIG->hMODE, FLEXModes[Mode]);
			strcpy(RIG->WEB_MODE, FLEXModes[Mode]);
		}
		else
		{
			Mode = Msg[2] - 48;
			if (Mode > 7) Mode = 7;
			SetWindowText(RIG->hMODE, KenwoodModes[Mode]);
			strcpy(RIG->WEB_MODE, KenwoodModes[Mode]);
		}
	}

	if (CmdLen < Length)
	{
		// Another Message in Buffer

		ptr++;
		Length -= (ptr - Msg);

		if (Length <= 0)
			return;

		memmove(Msg, ptr, Length +1);

		goto Loop;
	}
}


VOID KenwoodPoll(struct RIGPORTINFO * PORT)
{
	UCHAR * Poll = PORT->TXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];		// Only one on Kenwood

	if (RIG->ScanStopped == 0)
		if (RIG->ScanCounter)
			RIG->ScanCounter--;

	if (PORT->Timeout)
	{
		PORT->Timeout--;
		
		if (PORT->Timeout)			// Still waiting
			return;

		PORT->Retries--;

		if(PORT->Retries)
		{
			RigWriteCommBlock(PORT);	// Retransmit Block
			return;
		}

		SetWindowText(RIG->hFREQ, "------------------");
		SetWindowText(RIG->hMODE, "----------");
		strcpy(RIG->WEB_FREQ, "-----------");;
		strcpy(RIG->WEB_MODE, "------");

		RIG->RIGOK = FALSE;

		return;
	}

	// Send Data if avail, else send poll

	if (RIG->NumberofBands && RIG->RIGOK && (RIG->ScanStopped == 0))
	{
		if (RIG->ScanCounter <= 0)
		{
			//	Send Next Freq

			if (GetPermissionToChange(PORT, RIG))
			{
				if (RIG_DEBUG)
					Debugprintf("BPQ32 Change Freq to %9.4f", PORT->FreqPtr->Freq);

				memcpy(PORT->TXBuffer, PORT->FreqPtr->Cmd1, PORT->FreqPtr->Cmd1Len);
				PORT->TXLen = PORT->FreqPtr->Cmd1Len;

				RigWriteCommBlock(PORT);
				PORT->CmdSent = 1;
				PORT->Retries = 0;	
				PORT->Timeout = 0;
				PORT->AutoPoll = TRUE;

				// There isn't a response to a set command, so clear Scan Lock here
			
				ReleasePermission(RIG);			// Release Perrmission

			return;
			}
		}
	}
	
	if (RIG->RIGOK && RIG->BPQtoRADIO_Q)
	{
		int datalen;
		UINT * buffptr;
			
		buffptr=Q_REM(&RIG->BPQtoRADIO_Q);

		datalen=buffptr[1];

		// Copy the ScanEntry struct from the Buffer to the PORT Scanentry

		memcpy(&PORT->ScanEntry, buffptr+2, sizeof(struct ScanEntry));

		PORT->FreqPtr = &PORT->ScanEntry;		// Block we are currently sending.
		
		if (RIG_DEBUG)
			Debugprintf("BPQ32 Change Freq to %9.4f", PORT->FreqPtr->Freq);

		DoBandwidthandAntenna(RIG, &PORT->ScanEntry);

		memcpy(Poll, &buffptr[20], datalen);

		PORT->TXLen = datalen;
		RigWriteCommBlock(PORT);
		PORT->CmdSent = Poll[4];
		PORT->Timeout = 0;
		RIG->PollCounter = 10;

		ReleaseBuffer(buffptr);
		PORT->AutoPoll = FALSE;
	
		return;
	}
		
	if (RIG->PollCounter)
	{
		RIG->PollCounter--;
		if (RIG->PollCounter > 1)
			return;
	}

	if (RIG->RIGOK && (RIG->ScanStopped == 0) && RIG->NumberofBands)
		return;						// no point in reading freq if we are about to change it

	RIG->PollCounter = 10;			// Once Per Sec
		
	// Read Frequency 

	PORT->TXLen = RIG->PollLen;
	strcpy(Poll, RIG->Poll);
	
	RigWriteCommBlock(PORT);
	PORT->Retries = 1;
	PORT->Timeout = 10;
	PORT->CmdSent = 0;

	PORT->AutoPoll = TRUE;

	return;
}

VOID DummyPoll(struct RIGPORTINFO * PORT)
{
	UCHAR * Poll = PORT->TXBuffer;
	struct RIGINFO * RIG = &PORT->Rigs[0];

	if (RIG->ScanStopped == 0)
		if (RIG->ScanCounter)
			RIG->ScanCounter--;
	
	if (RIG->NumberofBands && RIG->RIGOK && (RIG->ScanStopped == 0))
	{
		if (RIG->ScanCounter <= 0)
		{
			//	Send Next Freq

			if (GetPermissionToChange(PORT, RIG))
			{
				if (RIG_DEBUG)
					Debugprintf("BPQ32 Change Freq to %9.4f", PORT->FreqPtr->Freq);

				memcpy(PORT->TXBuffer, PORT->FreqPtr->Cmd1, PORT->FreqPtr->Cmd1Len);
				PORT->TXLen = PORT->FreqPtr->Cmd1Len;
/*
				RigWriteCommBlock(PORT);
				PORT->CmdSent = 1;
				PORT->Retries = 0;	
				PORT->Timeout = 0;
				PORT->AutoPoll = TRUE;
*/
				// There isn't a response to a set command, so clear Scan Lock here
			
				ReleasePermission(RIG);			// Release Perrmission

			return;
			}
		}
	}
	
	return;
}

VOID SwitchAntenna(struct RIGINFO * RIG, char Antenna)
{
	struct RIGPORTINFO * PORT;
	char Ant[3]="  ";

	if (RIG == NULL) return;

	PORT = RIG->PORT;

	Ant[1] = Antenna;

	SetWindowText(RIG->hPTT, Ant);

	switch (Antenna)
	{
	case '1':
		COMClearDTR(PORT->hDevice);
		COMClearRTS(PORT->hDevice);
		break;
	case '2':
		COMSetDTR(PORT->hDevice);
		COMClearRTS(PORT->hDevice);
		break;
	case '3':
		COMClearDTR(PORT->hDevice);
		COMSetRTS(PORT->hDevice);
		break;
	case '4':
		COMSetDTR(PORT->hDevice);
		COMSetRTS(PORT->hDevice);
		break;
	}	
}

BOOL DecodeModePtr(char * Param, double * Dwell, double * Freq, char * Mode,
				   char * PMinLevel, char * PMaxLevel, char * PacketMode,
				   char * RPacketMode, char * Split, char * Data, char * WinmorMode,
				   char * Antenna, BOOL * Supress, char * Filter, char * Appl,
				   char * MemoryBank, int * MemoryNumber)
{
	char * Context;
	char * ptr;

	*Filter = '1';			// Defaults
	*PMinLevel = 1;
	*MemoryBank = 0;
	*MemoryNumber = 0;
	*Mode = 0;
	
	ptr = strtok_s(Param, ",", &Context);

	// "New" format - Dwell, Freq, Params.
	
	//	Each param is a 2 char pair, separated by commas

	// An - Antenna
	// Pn - Pactor
	// Wn - Winmor
	// Pn - Packet
	// Fn - Filter
	// Sx - Split

	// 7.0770/LSB,F1,A3,WN,P1,R1 

	*Dwell = atof(ptr);
	
	ptr = strtok_s(NULL, ",", &Context);

	// May be a frequency or a Memory Bank/Channel 

	if (_memicmp(ptr, "Chan", 4) == 0)
	{
		if (strchr(ptr, '/'))		// Bank/Chan
		{
			memcpy(MemoryBank, &ptr[4], 1);
			*MemoryNumber = atoi(&ptr[6]);
		}
		else
			*MemoryNumber = atoi(&ptr[4]);	// Just Chan

		*Freq = 0.0;
	}
	else
		*Freq = atof(ptr);

	ptr = strtok_s(NULL, ",", &Context);

	if (strlen(ptr) >  6)
		return FALSE;

	// If channel, dont need mode

	if (*MemoryNumber == 0)
	{
		strcpy(Mode, ptr); 
		ptr = strtok_s(NULL, ",", &Context);
	}

	while (ptr)
	{
		if (memcmp(ptr, "APPL=", 5) == 0)
		{
			strcpy(Appl, ptr + 5);
		}
		else if (ptr[0] == 'A')
			*Antenna = ptr[1];
		
		else if (ptr[0] == 'F')
			*Filter = ptr[1];

		else if (ptr[0] == 'R')
			*RPacketMode = ptr[1];
		
		else if (ptr[0] == 'H')
			*PacketMode = ptr[1];

		else if (ptr[0] == 'N')
			*PacketMode = ptr[1];

		else if (ptr[0] == 'P')
		{
			*PMinLevel = ptr[1];
			*PMaxLevel = ptr[strlen(ptr) - 1];
		}
		else if (ptr[0] == 'W')
		{
			*WinmorMode = ptr[1];
			if (*WinmorMode == '0')
				*WinmorMode = 'X';
			else if (*WinmorMode == '1')
				*WinmorMode = 'N';
			else if (*WinmorMode == '2')
				*WinmorMode = 'W';
		}

		else if (ptr[0] == '+')
			*Split = '+';
		else if (ptr[0] == '-')
			*Split = '-';
		else if (ptr[0] == 'S')
			*Split = 'S';
		else if (ptr[0] == 'D')
			*Data = 1;
		else if (ptr[0] == 'X')
			*Supress = TRUE;

		ptr = strtok_s(NULL, ",", &Context);
	}
	return TRUE;

}

VOID AddNMEAChecksum(char * msg)
{
	UCHAR CRC = 0;

	msg++;					// Skip $

	while (*(msg) != '*')
	{
		CRC ^= *(msg++);
	}

	sprintf(++msg, "%02X\r\n", CRC);
}

// Called by Port Driver .dll to add/update rig info 

// RIGCONTROL COM60 19200 ICOM IC706 5e 4 14.103/U1w 14.112/u1 18.1/U1n 10.12/l1


struct RIGINFO * RigConfig(struct TNCINFO * TNC, char * buf, int Port)
{
	int i;
	char * ptr;
	char * COMPort = NULL;
	char * RigName;
	int RigAddr;
	struct RIGPORTINFO * PORT;
	struct RIGINFO * RIG;
	struct ScanEntry ** FreqPtr;
	char * CmdPtr;
	char * Context;
	struct TimeScan * SaveBand;
	char PTTRigName[] = "PTT";
	double ScanFreq;
	double Dwell;
	char MemoryBank;	// For Memory Scanning
	int MemoryNumber;

	BOOL PTTControlsInputMUX = FALSE;
	BOOL DataPTT = FALSE;
	int DataPTTOffMode = 1;				// ACC

	Debugprintf("Processing RIG line %s", buf);

	ptr = strtok_s(&buf[10], " \t\n\r", &Context);

	if (ptr == NULL) return FALSE;

	if (_memicmp(ptr, "DEBUG", 5) == 0)
	{
		ptr = strtok_s(NULL, " \t\n\r", &Context);
		RIG_DEBUG = TRUE;
	}

	if (_memicmp(ptr, "AUTH", 4) == 0)
	{
		ptr = strtok_s(NULL, " \t\n\r", &Context);
		if (ptr == NULL) return FALSE;
		if (strlen(ptr) > 100) return FALSE;

		strcpy(AuthPassword, ptr);
		ptr = strtok_s(NULL, " \t\n\r", &Context);
	}

	if (ptr == NULL || ptr[0] == 0)
		return FALSE;

	if (_memicmp(ptr, "DUMMY", 5) == 0)
	{
		// Dummy to allow PTC application scanning

		PORT = PORTInfo[NumberofPorts++] = zalloc(sizeof(struct RIGPORTINFO));
		PORT->PortType = DUMMY;
		PORT->ConfiguredRigs = 1;
		RIG = &PORT->Rigs[0];
		RIG->PortNum = Port;
		RIG->BPQPort |=  (1 << Port);
		RIG->RIGOK = TRUE;

		ptr = strtok_s(NULL, " \t\n\r", &Context);

		goto CheckScan;
	}

	if ((_memicmp(ptr, "VCOM", 4) == 0) && TNC->Hardware == H_SCS)		// Using Radio Port on PTC
		COMPort = 0;
	else if ((_memicmp(ptr, "PTCPORT", 7) == 0) && TNC->Hardware == H_SCS)
		COMPort = 0;
	else
		COMPort = ptr;

	// See if port is already defined. We may be adding another radio (ICOM only) or updating an existing one

	for (i = 0; i < NumberofPorts; i++)
	{
		PORT = PORTInfo[i];

		if (COMPort)
			if (strcmp(PORT->IOBASE, COMPort) == 0)
				goto PortFound;
	
		if (COMPort == 0)
			if (PORT->IOBASE == COMPort)
				goto PortFound;
	}

	// Allocate a new one

	PORT = PORTInfo[NumberofPorts++] = zalloc(sizeof(struct RIGPORTINFO));

	if (COMPort)
		strcpy(PORT->IOBASE, COMPort);
	else
		PORT->PTC = TNC;

PortFound:

	_strupr(Context);

	ptr = strtok_s(NULL, " \t\n\r", &Context);

	if (ptr == NULL) return (FALSE);

	PORT->SPEED = atoi(ptr);

	ptr = strtok_s(NULL, " \t\n\r", &Context);
	if (ptr == NULL) return (FALSE);

	if (memcmp(ptr, "PTTCOM", 6) == 0 || memcmp(ptr, "PTT=", 4) == 0)
	{
		strcpy(PORT->PTTIOBASE, ptr);
		ptr = strtok_s(NULL, " \t\n\r", &Context);
		if (ptr == NULL) return (FALSE);
	}

//		if (strcmp(ptr, "ICOM") == 0 || strcmp(ptr, "YAESU") == 0 
//			|| strcmp(ptr, "KENWOOD") == 0 || strcmp(ptr, "PTTONLY") == 0 || strcmp(ptr, "ANTENNA") == 0)

			// RADIO IC706 4E 5 14.103/U1 14.112/u1 18.1/U1 10.12/l1
			// Read RADIO Lines

	if (strcmp(ptr, "ICOM") == 0)
		PORT->PortType = ICOM;
	else
	if (strcmp(ptr, "YAESU") == 0)
		PORT->PortType = YAESU;
	else
	if (strcmp(ptr, "KENWOOD") == 0)
		PORT->PortType = KENWOOD;
	else
	if (strcmp(ptr, "FLEX") == 0)
		PORT->PortType = FLEX;
	else
	if (strcmp(ptr, "NMEA") == 0)
		PORT->PortType = NMEA;
	else
	if (strcmp(ptr, "PTTONLY") == 0)
		PORT->PortType = PTT;
	else
	if (strcmp(ptr, "ANTENNA") == 0)
		PORT->PortType = ANT;
	else
		return FALSE;

	Debugprintf("Port type = %d", PORT->PortType);

	ptr = strtok_s(NULL, " \t\n\r", &Context);

	if (ptr == NULL)
	{
		if (PORT->PortType == PTT)
			ptr = PTTRigName;
		else
			return FALSE;
	}

	if (strlen(ptr) > 9) return FALSE;
	
	RigName =  ptr;

	Debugprintf("Rigname = *%s*", RigName);

	// FT100 seems to be different to most other YAESU types

	if (strcmp(RigName, "FT100") == 0 && PORT->PortType == YAESU)
	{
		PORT->PortType = FT100;
	}

	// FT100 seems to be different to most other YAESU types

	if (strcmp(RigName, "FT990") == 0 && PORT->PortType == YAESU)
	{
		PORT->PortType = FT990;
	}

	// FT1000 seems to be different to most other YAESU types

	if (strstr(RigName, "FT1000") && PORT->PortType == YAESU)
	{
		PORT->PortType = FT1000;

		// Subtypes need different code. D and no suffix are same

		if (strstr(RigName, "FT1000MP"))
			PORT->YaesuVariant = FT1000MP;
		else
			PORT->YaesuVariant = FT1000D;
	}

	// FT2000 seems to be different to most other YAESU types

	if (strcmp(RigName, "FT2000") == 0 && PORT->PortType == YAESU)
	{
		PORT->PortType = FT2000;
	}

	// If PTTONLY, may be defining another rig using the other control line

	if (PORT->PortType == PTT)
	{
		// See if already defined

		for (i = 0; i < PORT->ConfiguredRigs; i++)
		{
			RIG = &PORT->Rigs[i];

			if (RIG->PortNum == Port)
				goto PTTFound;
		}

		// Allocate a new one

		RIG = &PORT->Rigs[PORT->ConfiguredRigs++];

	PTTFound:

		strcpy(RIG->RigName, RigName);
		RIG->PortNum = Port;
		RIG->BPQPort |=  (1 << Port);

//		return RIG;
	}

	// If ICOM, we may be adding a new Rig

	ptr = strtok_s(NULL, " \t\n\r", &Context);

	if (PORT->PortType == ICOM || PORT->PortType == NMEA)
	{
		if (ptr == NULL) return (FALSE);
		sscanf(ptr, "%x", &RigAddr);

		// See if already defined

		for (i = 0; i < PORT->ConfiguredRigs; i++)
		{
			RIG = &PORT->Rigs[i];

			if (RIG->RigAddr == RigAddr)
				goto RigFound;
		}

		// Allocate a new one

		RIG = &PORT->Rigs[PORT->ConfiguredRigs++];
		RIG->RigAddr = RigAddr;

	RigFound:

		ptr = strtok_s(NULL, " \t\n\r", &Context);
//		if (ptr == NULL) return (FALSE);
	}
	else
	{
		// Only allows one RIG

		PORT->ConfiguredRigs = 1;
		RIG = &PORT->Rigs[0];
	}
	
	strcpy(RIG->RigName, RigName);

	RIG->PortNum = Port;
	RIG->BPQPort |=  (1 << Port);

	while (ptr)
	{
		if (strcmp(ptr, "PTTMUX") == 0)
		{
			// Ports whose RTS/DTR will be converted to CAT commands (for IC7100/IC7200 etc)
	
			int PIndex = 0;

			ptr = strtok_s(NULL, " \t\n\r", &Context);

			while (memcmp(ptr, "COM", 3) == 0)
			{	
				strcpy(RIG->PTTCATPort[PIndex], &ptr[3]);
				
				if (PIndex < 3)
					PIndex++;

				ptr = strtok_s(NULL, " \t\n\r", &Context);
		
				if (ptr == NULL)
					break;
			}
			if (ptr == NULL)
				break;
		}
		if (strcmp(ptr, "PTT_SETS_INPUT") == 0)
		{
			// Send Select Soundcard as mod source with PTT commands

			PTTControlsInputMUX = TRUE;

			// See if following param is an PTT Off Mode

			ptr = strtok_s(NULL, " \t\n\r", &Context);

			if (ptr == NULL)
				break;
	
			if (strcmp(ptr, "MIC") == 0)
				DataPTTOffMode = 0;
			else if (strcmp(ptr, "AUX") == 0)
				DataPTTOffMode = 1;
			else if (strcmp(ptr, "MICAUX") == 0)
				DataPTTOffMode = 2;
			else
				continue;

			ptr = strtok_s(NULL, " \t\n\r", &Context);

			continue;

		}
		if (strcmp(ptr, "DATAPTT") == 0)
		{
			// Send Select Soundcard as mod source with PTT commands

			DataPTT = TRUE;
		}
		else if (atoi(ptr))
			break;					// Not scan freq oe timeband, so see if another param

		ptr = strtok_s(NULL, " \t\n\r", &Context);
	}

	if (PORT->PortType == PTT || PORT->PortType == ANT)
		return RIG;

	// Set up PTT and Poll Strings

	if (PORT->PortType == ICOM)
	{
		char * Poll;
		Poll = &RIG->PTTOn[0];

		if (PTTControlsInputMUX)
		{
			*(Poll++) = 0xFE;
			*(Poll++) = 0xFE;
			*(Poll++) = RIG->RigAddr;
			*(Poll++) = 0xE0;
			*(Poll++) = 0x1a;

			if (strcmp(RIG->RigName, "IC7100") == 0)
			{
				*(Poll++) = 0x05;
				*(Poll++) = 0x00;
				*(Poll++) = 0x91;			// Data Mode Source
			}
			else if (strcmp(RIG->RigName, "IC7200") == 0)
			{
				*(Poll++) = 0x03;
				*(Poll++) = 0x24;			// Data Mode Source
			}

			else if (strcmp(RIG->RigName, "IC7410") == 0)
			{
				*(Poll++) = 0x03;
				*(Poll++) = 0x39;			// Data Mode Source
			}

			*(Poll++) = 0x03;			// USB Soundcard
			*(Poll++) = 0xFD;
		}

		*(Poll++) = 0xFE;
		*(Poll++) = 0xFE;
		*(Poll++) = RIG->RigAddr;
		*(Poll++) = 0xE0;
		*(Poll++) = 0x1C;		// RIG STATE
		*(Poll++) = 0x00;		// PTT
		*(Poll++) = 1;			// ON
		*(Poll++) = 0xFD;

		RIG->PTTOnLen = Poll - &RIG->PTTOn[0];

		Poll = &RIG->PTTOff[0];

		*(Poll++) = 0xFE;
		*(Poll++) = 0xFE;
		*(Poll++) = RIG->RigAddr;
		*(Poll++) = 0xE0;
		*(Poll++) = 0x1C;		// RIG STATE
		*(Poll++) = 0x00;		// PTT
		*(Poll++) = 0;			// OFF
		*(Poll++) = 0xFD;

		if (PTTControlsInputMUX)
		{
			*(Poll++) = 0xFE;
			*(Poll++) = 0xFE;
			*(Poll++) = RIG->RigAddr;
			*(Poll++) = 0xE0;
			*(Poll++) = 0x1a;

			if (strcmp(RIG->RigName, "IC7100") == 0)
			{
				*(Poll++) = 0x05;
				*(Poll++) = 0x00;
				*(Poll++) = 0x91;			// Data Mode Source
			}
			else if (strcmp(RIG->RigName, "IC7200") == 0)
			{
				*(Poll++) = 0x03;
				*(Poll++) = 0x24;			// Data Mode Source
			}
			else if (strcmp(RIG->RigName, "IC7410") == 0)
			{
				*(Poll++) = 0x03;
				*(Poll++) = 0x39;			// Data Mode Source
			}

			*(Poll++) = DataPTTOffMode;
			*(Poll++) = 0xFD;
		}

		RIG->PTTOffLen = Poll - &RIG->PTTOff[0];

	}
	else if	(PORT->PortType == KENWOOD)
	{	
		RIG->PollLen = 6;
		strcpy(RIG->Poll, "FA;MD;");

		if (PTTControlsInputMUX)
		{
			strcpy(RIG->PTTOn, "EX06300001;TX1;");			// Select USB before PTT
			strcpy(RIG->PTTOff, "RX;EX06300000;");			// Select ACC after dropping PTT
		}
		else
		{
			strcpy(RIG->PTTOff, "RX;");
			
			if (DataPTT)
				strcpy(RIG->PTTOn, "TX1;");
			else
				strcpy(RIG->PTTOn, "TX;");
		}

		RIG->PTTOnLen = strlen(RIG->PTTOn);
		RIG->PTTOffLen = strlen(RIG->PTTOff);

	}
	else if	(PORT->PortType == FLEX)
	{	
		RIG->PollLen = 10;
		strcpy(RIG->Poll, "ZZFA;ZZMD;");

		strcpy(RIG->PTTOn, "ZZTX1;");
		RIG->PTTOnLen = 6;
		strcpy(RIG->PTTOff, "ZZTX0;");
		RIG->PTTOffLen = 6;
	}
	else if	(PORT->PortType == FT2000)
	{	
		RIG->PollLen = 6;
		strcpy(RIG->Poll, "FA;MD;");

		strcpy(RIG->PTTOn, "TX1;");
		RIG->PTTOnLen = 4;
		strcpy(RIG->PTTOff, "TX0;");
		RIG->PTTOffLen = 4;
	}
	else if	(PORT->PortType == NMEA)
	{	
		int Len;
			
		i = sprintf(RIG->Poll, "$PICOA,90,%02x,RXF*xx\r\n", RIG->RigAddr);
		AddNMEAChecksum(RIG->Poll);
		Len = i;
		i = sprintf(RIG->Poll + Len, "$PICOA,90,%02x,MODE*xx\r\n", RIG->RigAddr);
		AddNMEAChecksum(RIG->Poll + Len);
		RIG->PollLen = Len + i;

		i = sprintf(RIG->PTTOn, "$PICOA,90,%02x,TRX,TX*xx\r\n", RIG->RigAddr);
		AddNMEAChecksum(RIG->PTTOn);
		RIG->PTTOnLen = i;

		i = sprintf(RIG->PTTOff, "$PICOA,90,%02x,TRX,RX*xx\r\n", RIG->RigAddr);
		AddNMEAChecksum(RIG->PTTOff);
		RIG->PTTOffLen = i;
	}


	if (ptr == NULL) return RIG;			// No Scanning, just Interactive control
	
	if (strchr(ptr, ',') == 0)				// Old Format
	{
		ScanFreq = atof(ptr);

		#pragma warning(push)
		#pragma warning(disable : 4244)

		RIG->ScanFreq = ScanFreq * 10;

		#pragma warning(push)

		ptr = strtok_s(NULL, " \t\n\r", &Context);
	}

	// Frequency List

CheckScan:

	if (ptr)
		if (ptr[0] == ';' || ptr[0] == '#')
			ptr = NULL;

	if (ptr != NULL)
	{
		// Create Default Timeband

		struct TimeScan * Band = AllocateTimeRec(RIG);
		SaveBand = Band;

		Band->Start = 0;
		Band->End = 84540;	//23:59
		FreqPtr = Band->Scanlist = RIG->FreqPtr = malloc(1000);
		memset(FreqPtr, 0, 1000);
	}

	while(ptr)
	{
		int ModeNo;
		BOOL Supress;
		double Freq = 0.0;
		char FreqString[80]="";
		char * Valchar;
		char * Modeptr = NULL;
		int dec, sign;
		char Split, Data, PacketMode, RPacketMode, PMinLevel, PMaxLevel, Filter;
		char Mode[10] = "";
		char WinmorMode, Antenna;
		char Appl[13];
		char * ApplCall;

		if (ptr[0] == ';' || ptr[0] == '#')
			break;

		Filter = PMinLevel = PMaxLevel = PacketMode = RPacketMode = Split =
			Data = WinmorMode = Antenna = ModeNo = Supress = MemoryNumber = 0;
	
		MemoryBank = 0;
		Appl[0] = 0;

		Dwell = 0.0;

		if (strchr(ptr, ':'))
		{
			// New TimeBand

			struct TimeScan * Band;
						
			Band = AllocateTimeRec(RIG);

			*FreqPtr = (struct ScanEntry *)0;		// Terminate Last Band
						
			Band->Start = (atoi(ptr) * 3600) + (atoi(&ptr[3]) * 60);
			Band->End = 84540;	//23:59
			SaveBand->End = Band->Start - 60;

			SaveBand = Band;

			FreqPtr = Band->Scanlist = RIG->FreqPtr = malloc(1000);
			memset(FreqPtr, 0, 1000);

			ptr = strtok_s(NULL, " \t\n\r", &Context);										
		}

		if (strchr(ptr, ','))			// New Format
		{
			DecodeModePtr(ptr, &Dwell, &Freq, Mode, &PMinLevel, &PMaxLevel, &PacketMode,
				&RPacketMode, &Split, &Data, &WinmorMode, &Antenna, &Supress, &Filter, &Appl[0],
				&MemoryBank, &MemoryNumber);
		}
		else
		{
			Modeptr = strchr(ptr, '/');
					
			if (Modeptr)
				*Modeptr++ = 0;

			Freq = atof(ptr);

			if (Modeptr)
			{
				// Mode can include 1/2/3 for Icom Filers. W/N for Winmor/Pactor Bandwidth, and +/-/S for Repeater Shift (S = Simplex) 
				// First is always Mode
				// First char is Mode (USB, LSB etc)

				Mode[0] = Modeptr[0];
				Filter = Modeptr[1];

				if (strchr(&Modeptr[1], '+'))
					Split = '+';
				else if (strchr(&Modeptr[1], '-'))
					Split = '-';
				else if (strchr(&Modeptr[1], 'S'))
					Split = 'S';
				else if (strchr(&Modeptr[1], 'D'))
					Data = 1;
						
				if (strchr(&Modeptr[1], 'W'))
				{
					WinmorMode = 'W';
					PMaxLevel = '3';
					PMinLevel = '1';
				}
				else if (strchr(&Modeptr[1], 'N'))
				{
					WinmorMode = 'N';
					PMaxLevel = '2';
					PMinLevel = '1';
				}

				if (strchr(&Modeptr[1], 'R'))		// Robust Packet
					RPacketMode = '2';				// R600
				else if (strchr(&Modeptr[1], 'H'))	// HF Packet on Tracker
					PacketMode = '1';				// 300

				if (strchr(&Modeptr[1], 'X'))		// Dont Report to WL2K
					Supress = 1;

				if (strstr(&Modeptr[1], "A1"))
						Antenna = '1';
				else if (strstr(&Modeptr[1], "A2"))
					Antenna = '2';
				if (strstr(&Modeptr[1], "A3"))
					Antenna = '3';
				else if (strstr(&Modeptr[1], "A4"))
					Antenna = '4';
				}
			}
			
			switch(PORT->PortType)
			{
			case ICOM:						
						
				for (ModeNo = 0; ModeNo < 8; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (Modes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(Modes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;

			case YAESU:						
						
				for (ModeNo = 0; ModeNo < 16; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (YaesuModes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(YaesuModes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;

			case KENWOOD:
						
				for (ModeNo = 0; ModeNo < 8; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (KenwoodModes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(KenwoodModes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;

			case FLEX:
						
				for (ModeNo = 0; ModeNo < 12; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (FLEXModes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(FLEXModes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;

			case FT2000:
						
				if (Modeptr)
				{
					if (strstr(Modeptr, "PL"))
					{
						ModeNo = 8;
						break;
					}
					if (strstr(Modeptr, "PU"))
					{
						ModeNo = 12;
						break;
					}
				}
				for (ModeNo = 0; ModeNo < 14; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (FT2000Modes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(FT2000Modes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;

			case FT100:						
				
				for (ModeNo = 0; ModeNo < 8; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (FT100Modes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(FT100Modes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;

			case FT990:	

				for (ModeNo = 0; ModeNo < 12; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (FT990Modes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(FT990Modes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;

			case FT1000:						
				
				for (ModeNo = 0; ModeNo < 12; ModeNo++)
				{
					if (strlen(Mode) == 1)
					{
						if (FT1000Modes[ModeNo][0] == Mode[0])
							break;
					}
					else
					{
						if (_stricmp(FT1000Modes[ModeNo], Mode) == 0)
							break;
					}
				}
				break;
		}

		Freq = Freq * 1000000.0;


		Valchar = _fcvt(Freq, 0, &dec, &sign);

		if (dec == 9)	// 10-100
			sprintf(FreqString, "%s", Valchar);
		else
		if (dec == 8)	// 10-100
			sprintf(FreqString, "0%s", Valchar);		
		else
		if (dec == 7)	// 1-10
			sprintf(FreqString, "00%s", Valchar);
		else
		if (dec == 6)	// 0.1 - 1
			sprintf(FreqString, "000%s", Valchar);
		else
		if (dec == 5)	// 0.01 - .1
			sprintf(FreqString, "000%s", Valchar);

		FreqPtr[0] = malloc(sizeof(struct ScanEntry));
		memset(FreqPtr[0], 0, sizeof(struct ScanEntry));

		#pragma warning(push)
		#pragma warning(disable : 4244)

		if (Dwell == 0.0)
			FreqPtr[0]->Dwell = ScanFreq * 10;
		else
			FreqPtr[0]->Dwell = Dwell * 10;

		#pragma warning(pop) 

		FreqPtr[0]->Freq = Freq;
		FreqPtr[0]->Bandwidth = WinmorMode;
		FreqPtr[0]->RPacketMode = RPacketMode;
		FreqPtr[0]->HFPacketMode = PacketMode;
		FreqPtr[0]->PMaxLevel = PMaxLevel;
		FreqPtr[0]->PMinLevel = PMinLevel;
		FreqPtr[0]->Antenna = Antenna;
//		FreqPtr[0]->Supress = Supress;

		strcpy(FreqPtr[0]->APPL, Appl);

		ApplCall = GetApplCallFromName(Appl);

		if (strcmp(Appl, "NODE") == 0)
		{
			memcpy(FreqPtr[0]->APPLCALL, TNC->NodeCall, 9);
			strlop(FreqPtr[0]->APPLCALL, ' ');
		}
		else
		{
			if (ApplCall && ApplCall[0] > 32)
			{
				memcpy(FreqPtr[0]->APPLCALL, ApplCall, 9);
				strlop(FreqPtr[0]->APPLCALL, ' ');
			}
		}

		CmdPtr = FreqPtr[0]->Cmd1 = malloc(100);

		if (PORT->PortType == ICOM)
		{
			*(CmdPtr++) = 0xFE;
			*(CmdPtr++) = 0xFE;
			*(CmdPtr++) = RIG->RigAddr;
			*(CmdPtr++) = 0xE0;

			if (MemoryNumber)
			{
				// Set Memory Channel instead of Freq, Mode, etc

				char ChanString[5];

				// Send Set Memory, then Channel
								
				*(CmdPtr++) = 0x08;
				*(CmdPtr++) = 0xFD;

				*(CmdPtr++) = 0xFE;
				*(CmdPtr++) = 0xFE;
				*(CmdPtr++) = RIG->RigAddr;
				*(CmdPtr++) = 0xE0;

				sprintf(ChanString, "%04d", MemoryNumber); 
	
				*(CmdPtr++) = 0x08;
				*(CmdPtr++) = (ChanString[1] - 48) | ((ChanString[0] - 48) << 4);
				*(CmdPtr++) = (ChanString[3] - 48) | ((ChanString[2] - 48) << 4);
				*(CmdPtr++) = 0xFD;
				
				FreqPtr[0]->Cmd1Len = 14;

				if (MemoryBank)
				{						
					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = RIG->RigAddr;
					*(CmdPtr++) = 0xE0;
					*(CmdPtr++) = 0x08;
					*(CmdPtr++) = 0xA0;
					*(CmdPtr++) = MemoryBank - 0x40;
					*(CmdPtr++) = 0xFD;

					FreqPtr[0]->Cmd1Len += 8;
				}
			}	
			else
			{
				*(CmdPtr++) = 0x5;		// Set frequency command

				// Need to convert two chars to bcd digit
		
				*(CmdPtr++) = (FreqString[8] - 48) | ((FreqString[7] - 48) << 4);
				*(CmdPtr++) = (FreqString[6] - 48) | ((FreqString[5] - 48) << 4);
				*(CmdPtr++) = (FreqString[4] - 48) | ((FreqString[3] - 48) << 4);
				*(CmdPtr++) = (FreqString[2] - 48) | ((FreqString[1] - 48) << 4);
				*(CmdPtr++) = (FreqString[0] - 48);

				*(CmdPtr++) = 0xFD;

				FreqPtr[0]->Cmd1Len = 11;
				
				// Send Set VFO in case last chan was memory
							
//				*(CmdPtr++) = 0xFE;
//				*(CmdPtr++) = 0xFE;
//				*(CmdPtr++) = RIG->RigAddr;
//				*(CmdPtr++) = 0xE0;

//				*(CmdPtr++) = 0x07;
//				*(CmdPtr++) = 0xFD;

//				FreqPtr[0]->Cmd1Len = 17;

				if (Filter)
				{						
					CmdPtr = FreqPtr[0]->Cmd2 = malloc(10);
					FreqPtr[0]->Cmd2Len = 8;		
					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = 0xFE;
					*(CmdPtr++) = RIG->RigAddr;
					*(CmdPtr++) = 0xE0;
					*(CmdPtr++) = 0x6;		// Set Mode
					*(CmdPtr++) = ModeNo;
					*(CmdPtr++) = Filter - '0'; //Filter
					*(CmdPtr++) = 0xFD;

					if (Split)
					{
						CmdPtr = FreqPtr[0]->Cmd3 = malloc(10);
						FreqPtr[0]->Cmd3Len = 7;
						*(CmdPtr++) = 0xFE;
						*(CmdPtr++) = 0xFE;
						*(CmdPtr++) = RIG->RigAddr;
						*(CmdPtr++) = 0xE0;
						*(CmdPtr++) = 0xF;		// Set Mode
						if (Split == 'S')
							*(CmdPtr++) = 0x10;
						else
							if (Split == '+')
								*(CmdPtr++) = 0x12;
						else
							if (Split == '-')
								*(CmdPtr++) = 0x11;
			
						*(CmdPtr++) = 0xFD;
					}
					else
					{
						if (Data)
						{
							CmdPtr = FreqPtr[0]->Cmd3 = malloc(10);

							*(CmdPtr++) = 0xFE;
							*(CmdPtr++) = 0xFE;
							*(CmdPtr++) = RIG->RigAddr;
							*(CmdPtr++) = 0xE0;
							*(CmdPtr++) = 0x1a;	

							if ((strcmp(RIG->RigName, "IC7100") == 0) || (strcmp(RIG->RigName, "IC7410") == 0))
							{
								FreqPtr[0]->Cmd3Len = 9;
								*(CmdPtr++) = 0x6;		// Send/read DATA mode with filter set
								*(CmdPtr++) = 0x1;		// Data On
								*(CmdPtr++) = Filter - '0'; //Filter
							}
							else if (strcmp(RIG->RigName, "IC7200") == 0)
							{
								FreqPtr[0]->Cmd3Len = 9;
								*(CmdPtr++) = 0x4;		// Send/read DATA mode with filter set
								*(CmdPtr++) = 0x1;		// Data On
								*(CmdPtr++) = Filter - '0'; //Filter
							}
							else
							{
								FreqPtr[0]->Cmd3Len = 8;
								*(CmdPtr++) = 0x6;		// Set Data
								*(CmdPtr++) = 0x1;		//On		
							}
						
							*(CmdPtr++) = 0xFD;
						}
					}
				}
			}
		}
		else if	(PORT->PortType == YAESU)
		{	
			//Send Mode first - changing mode can change freq

			*(CmdPtr++) = ModeNo;
			*(CmdPtr++) = 0;
			*(CmdPtr++) = 0;
			*(CmdPtr++) = 0;
			*(CmdPtr++) = 7;

			*(CmdPtr++) = (FreqString[1] - 48) | ((FreqString[0] - 48) << 4);
			*(CmdPtr++) = (FreqString[3] - 48) | ((FreqString[2] - 48) << 4);
			*(CmdPtr++) = (FreqString[5] - 48) | ((FreqString[4] - 48) << 4);
			*(CmdPtr++) = (FreqString[7] - 48) | ((FreqString[6] - 48) << 4);
			*(CmdPtr++) = 1;

			// FT847 Needs a Poll Here. Set up anyway, but only send if 847

			*(CmdPtr++) = 0;
			*(CmdPtr++) = 0;
			*(CmdPtr++) = 0;
			*(CmdPtr++) = 0;
			*(CmdPtr++) = 3;


		}
		else if	(PORT->PortType == KENWOOD)
		{	
			FreqPtr[0]->Cmd1Len = sprintf(CmdPtr, "FA00%s;MD%d;FA;MD;", FreqString, ModeNo);
		}
		else if	(PORT->PortType == FLEX)
		{	
			FreqPtr[0]->Cmd1Len = sprintf(CmdPtr, "ZZFA00%s;ZZMD%02d;ZZFA;ZZMD;", FreqString, ModeNo);
		}
		else if	(PORT->PortType == FT2000)
		{	
			FreqPtr[0]->Cmd1Len = sprintf(CmdPtr, "FA%s;MD0%X;FA;MD;", &FreqString[1], ModeNo);
		}
		else if	(PORT->PortType == FT100 || PORT->PortType == FT990
			|| PORT->PortType == FT1000)
		{
			// Allow Mode = "LEAVE" to suppress mode change

			//Send Mode first - changing mode can change freq

			if (strcmp(Mode, "LEAVE") == 0)
			{
				// we can't easily change the string length,
				// so just set freq twice

				*(CmdPtr++) = (FreqString[7] - 48) | ((FreqString[6] - 48) << 4);
				*(CmdPtr++) = (FreqString[5] - 48) | ((FreqString[4] - 48) << 4);
				*(CmdPtr++) = (FreqString[3] - 48) | ((FreqString[2] - 48) << 4);
				*(CmdPtr++) = (FreqString[1] - 48) | ((FreqString[0] - 48) << 4);
				*(CmdPtr++) = 10;
			}
			else
			{
				*(CmdPtr++) = 0;
				*(CmdPtr++) = 0;
				*(CmdPtr++) = 0;
				*(CmdPtr++) = ModeNo;
				*(CmdPtr++) = 12;
			}

			*(CmdPtr++) = (FreqString[7] - 48) | ((FreqString[6] - 48) << 4);
			*(CmdPtr++) = (FreqString[5] - 48) | ((FreqString[4] - 48) << 4);
			*(CmdPtr++) = (FreqString[3] - 48) | ((FreqString[2] - 48) << 4);
			*(CmdPtr++) = (FreqString[1] - 48) | ((FreqString[0] - 48) << 4);
			*(CmdPtr++) = 10;

			// Send Get Status, as these types doesn't ack commands

			*(CmdPtr++) = 0;
			*(CmdPtr++) = 0;
			*(CmdPtr++) = 0;
			
			if (PORT->PortType == FT990 || PORT->YaesuVariant == FT1000D)
				*(CmdPtr++) = 3;
			else
				*(CmdPtr++) = 2;		// F100 or FT1000MP
			
			*(CmdPtr++) = 16;			// Get Status
		}
		else if	(PORT->PortType == NMEA)
		{	
			int Len;
			
			i = sprintf(CmdPtr, "$PICOA,90,%02x,RXF,%.6f*xx\r\n", RIG->RigAddr, Freq/1000000.);
			AddNMEAChecksum(CmdPtr);
			Len = i;
			i = sprintf(CmdPtr + Len, "$PICOA,90,%02x,TXF,%.6f*xx\r\n", RIG->RigAddr, Freq/1000000.);
			AddNMEAChecksum(CmdPtr + Len);
			Len += i;
			i = sprintf(CmdPtr + Len, "$PICOA,90,%02x,MODE,%s*xx\r\n", RIG->RigAddr, Mode);
			AddNMEAChecksum(CmdPtr + Len);
			FreqPtr[0]->Cmd1Len = Len + i;

			i = sprintf(RIG->Poll, "$PICOA,90,%02x,RXF*xx\r\n", RIG->RigAddr);
			AddNMEAChecksum(RIG->Poll);
			Len = i;
			i = sprintf(RIG->Poll + Len, "$PICOA,90,%02x,MODE*xx\r\n", RIG->RigAddr);
			AddNMEAChecksum(RIG->Poll + Len);
			RIG->PollLen = Len + i;

		}

		FreqPtr++;

		RIG->ScanCounter = 20;

		ptr = strtok_s(NULL, " \t\n\r", &Context);		// Next Freq
	}

	return RIG;
}

VOID SetupScanInterLockGroups(struct RIGINFO *RIG)
{
	struct PORTCONTROL * PortRecord;

	// See if other ports in same scan/interlock group

	PortRecord = GetPortTableEntryFromPortNum(RIG->PortNum);

	if (PortRecord->PORTINTERLOCK)		// Port has Interlock defined
	{
		// Find records in same Interlock Group

		int LockGroup = PortRecord->PORTINTERLOCK;
					
		PortRecord = PORTTABLE;
					
		while (PortRecord)
		{
			if (LockGroup == PortRecord->PORTINTERLOCK)
				RIG->BPQPort |=  (1 << PortRecord->PORTNUMBER);

			PortRecord = PortRecord->PORTPOINTER;
		}
	}
}

VOID SetupPortRIGPointers()
{
	struct TNCINFO * TNC;
	int port;

// For each Winmor/Pactor port set up the TNC to RIG pointers

	for (port = 1; port < 33; port++)
	{
		TNC = TNCInfo[port];

		if (TNC == NULL)
			continue;

		if (TNC->RIG == NULL)
			TNC->RIG = Rig_GETPTTREC(port);		// Get from Intelock Port

		if (TNC->Hardware == H_WINMOR || TNC->Hardware == H_V4 || TNC->Hardware == H_ARDOP)
			if (TNC->RIG && TNC->PTTMode)
				TNC->RIG->PTTMode = TNC->PTTMode;
	
		if (TNC->RIG == NULL)
			TNC->RIG = &TNC->DummyRig;		// Not using Rig control, so use Dummy
	
/*		if (TNC->WL2KFreq[0])
		{
			// put in ValChar for MH reporting

			double Freq;

			Freq = atof(TNC->WL2KFreq) - 1500;
			Freq = Freq/1000000.;

			_gcvt(Freq, 9, TNC->RIG->Valchar);
			TNC->RIG->CurrentBandWidth = TNC->WL2KModeChar;
		}
*/
	}
}

#ifdef WIN32

VOID PTTCATThread(struct RIGINFO *RIG)
{
	DWORD dwLength = 0;
	int Length, ret, i;
	UCHAR * ptr1;
	UCHAR * ptr2;
	UCHAR c;
	UCHAR Block[4][80];
	UCHAR CurrentState[4] = {0};
#define RTS 2
#define DTR 4
	HANDLE Event;
	HANDLE Handle[4];
	OVERLAPPED Overlapped[4];
	char Port[32];
	int PIndex = 0;
	int HIndex = 0;

	EndPTTCATThread = FALSE;

	while (PIndex < 4 && RIG->PTTCATPort[PIndex][0])
	{
		sprintf(Port, "\\\\.\\pipe\\BPQCOM%s", RIG->PTTCATPort[PIndex]);

		Handle[HIndex] = CreateFile(Port, GENERIC_READ | GENERIC_WRITE,
                  0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

		if (Handle[HIndex] == (HANDLE) -1)
		{
			int Err = GetLastError();

			Consoleprintf("PTTMUX port COM%s Open failed code %d", RIG->PTTCATPort[PIndex], Err);
		}
		else
			HIndex++;

		PIndex++;

	}

	if (PIndex == 0)
		return;				// No ports

	Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	for (i = 0; i < HIndex; i ++)
	{
		// Prime a read on each handle

		memset(&Overlapped[i], 0, sizeof(OVERLAPPED));
		Overlapped[i].hEvent = Event;

		ReadFile(Handle[i], Block[i], 80, &Length, &Overlapped[i]);
	}
		
	while (EndPTTCATThread == FALSE)
	{

WaitAgain:

		ret = WaitForSingleObject(Event, 1000);

		if (ret == WAIT_TIMEOUT)
		{
			if (EndPTTCATThread)
			{
				for (i = 0; i < HIndex; i ++)
				{
					CancelIo(Handle[i]);
					CloseHandle(Handle[i]);
					Handle[i] = INVALID_HANDLE_VALUE;
				}
				CloseHandle(Event);
				return;
			}
			goto WaitAgain;
		}

		ResetEvent(Event);

		// See which request(s) have completed

		for (i = 0; i < HIndex; i ++)
		{
			ret =  GetOverlappedResult(Handle[i], &Overlapped[i], &Length, FALSE);

			if (ret)
			{
				ptr1 = Block[i];
				ptr2 = Block[i];

				while (Length > 0)
				{
					c = *(ptr1++);
				
					Length--;

					if (c == 0xff)
					{
						c = *(ptr1++);
						Length--;
					
						if (c == 0xff)			// ff ff means ff
						{
							Length--;
						}
						else
						{
							// This is connection / RTS/DTR statua from other end
							// Convert to CAT Command

							if (c == CurrentState[i])
								continue;

							if (c & RTS)
								Rig_PTT(RIG, TRUE);
							else
								Rig_PTT(RIG, FALSE);

							CurrentState[i] = c;
							continue;
						}
					}
				}
				
				memset(&Overlapped[i], 0, sizeof(OVERLAPPED));
				Overlapped[i].hEvent = Event;

				ReadFile(Handle[i], Block[i], 80, &Length, &Overlapped[i]);
			}
		}
	}
	EndPTTCATThread = FALSE;
}

/*
		memset(&Overlapped, 0, sizeof(Overlapped));
		Overlapped.hEvent = Event;
		ResetEvent(Event);

		ret = ReadFile(Handle, Block, 80, &Length, &Overlapped);
		
		if (ret == 0)
		{
			ret = GetLastError();

			if (ret != ERROR_IO_PENDING)
			{
				if (ret == ERROR_BROKEN_PIPE || ret == ERROR_INVALID_HANDLE)
				{
					CloseHandle(Handle);
					RIG->PTTCATHandles[0] = INVALID_HANDLE_VALUE;
					return;
				}
			}
		}
		
*/
#endif

/*
int CRow;

HANDLE hComPort, hSpeed, hRigType, hButton, hAddr, hLabel, hTimes, hFreqs, hBPQPort;

VOID CreateRigConfigLine(HWND hDlg, struct RIGPORTINFO * PORT, struct RIGINFO * RIG)
{
	char Port[10];

	hButton =  CreateWindow(WC_BUTTON , "", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_TABSTOP,
					10, CRow+5, 10,10, hDlg, NULL, hInstance, NULL);

	if (PORT->PortType == ICOM)
	{
		char Addr[10];

		sprintf(Addr, "%X", RIG->RigAddr);

		hAddr =  CreateWindow(WC_EDIT , Addr, WS_CHILD | WS_VISIBLE  | WS_TABSTOP | WS_BORDER,
                 305, CRow, 30,20, hDlg, NULL, hInstance, NULL);

	}
	hLabel =  CreateWindow(WC_EDIT , RIG->RigName, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER,
                 340, CRow, 60,20, hDlg, NULL, hInstance, NULL);

	sprintf(Port, "%d", RIG->PortRecord->PORTCONTROL.PORTNUMBER);
	hBPQPort =  CreateWindow(WC_EDIT , Port, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                 405, CRow, 20, 20, hDlg, NULL, hInstance, NULL);

	hTimes =  CreateWindow(WC_COMBOBOX , "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | 
                    WS_VSCROLL | WS_TABSTOP,
                 430, CRow, 100,80, hDlg, NULL, hInstance, NULL);

	hFreqs =  CreateWindow(WC_EDIT , RIG->FreqText, WS_CHILD | WS_VISIBLE| WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                 535, CRow, 300, 20, hDlg, NULL, hInstance, NULL);

	SendMessage(hTimes, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "0000:1159");
	SendMessage(hTimes, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "1200:2359");
	SendMessage(hTimes, CB_SETCURSEL, 0, 0);

	CRow += 30;	

}

VOID CreatePortConfigLine(HWND hDlg, struct RIGPORTINFO * PORT)
{	
	char Port[20]; 
	int i;

	hComPort =  CreateWindow(WC_COMBOBOX , "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | 
                    WS_VSCROLL | WS_TABSTOP,
                 30, CRow, 90, 160, hDlg, NULL, hInstance, NULL);

	for (i = 1; i < 256; i++)
	{
		sprintf(Port, "COM%d", i);
		SendMessage(hComPort, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) Port);
	}

	sprintf(Port, "COM%d", PORT->IOBASE);

	i = SendMessage(hComPort, CB_FINDSTRINGEXACT, 0,(LPARAM) Port);

	SendMessage(hComPort, CB_SETCURSEL, i, 0);
	
	
	hSpeed =  CreateWindow(WC_COMBOBOX , "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | 
                    WS_VSCROLL | WS_TABSTOP,
                 120, CRow, 75, 80, hDlg, NULL, hInstance, NULL);

	SendMessage(hSpeed, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "1200");
	SendMessage(hSpeed, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "2400");
	SendMessage(hSpeed, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "4800");
	SendMessage(hSpeed, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "9600");
	SendMessage(hSpeed, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "19200");
	SendMessage(hSpeed, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "38400");

	sprintf(Port, "%d", PORT->SPEED);

	i = SendMessage(hSpeed, CB_FINDSTRINGEXACT, 0, (LPARAM)Port);

	SendMessage(hSpeed, CB_SETCURSEL, i, 0);

	hRigType =  CreateWindow(WC_COMBOBOX , "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP,
                 200, CRow, 100,80, hDlg, NULL, hInstance, NULL);

	SendMessage(hRigType, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "ICOM");
	SendMessage(hRigType, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "YAESU");
	SendMessage(hRigType, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "KENWOOD");

	SendMessage(hRigType, CB_SETCURSEL, PORT->PortType -1, 0);

}

INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	int Cmd = LOWORD(wParam);

	switch (message)
	{
	case WM_INITDIALOG:
	{
		struct RIGPORTINFO * PORT;
		struct RIGINFO * RIG;
		int i, p;

		CRow = 40;

		for (p = 0; p < NumberofPorts; p++)
		{
			PORT = PORTInfo[p];

			CreatePortConfigLine(hDlg, PORT);
		
			for (i=0; i < PORT->ConfiguredRigs; i++)
			{
				RIG = &PORT->Rigs[i];
				CreateRigConfigLine(hDlg, PORT, RIG);
			}
		}



//	 CreateWindow(WC_STATIC , "",  WS_CHILD | WS_VISIBLE,
//                 90, Row, 40,20, hDlg, NULL, hInstance, NULL);
	
//	 CreateWindow(WC_STATIC , "",  WS_CHILD | WS_VISIBLE,
//                 135, Row, 100,20, hDlg, NULL, hInstance, NULL);

return TRUE; 
	}

	case WM_SIZING:
	{
		return TRUE;
	}

	case WM_ACTIVATE:

//		SendDlgItemMessage(hDlg, IDC_MESSAGE, EM_SETSEL, -1, 0);

		break;


	case WM_COMMAND:

		if (Cmd == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}

		return (INT_PTR)TRUE;

		break;
	}
	return (INT_PTR)FALSE;
}

*/