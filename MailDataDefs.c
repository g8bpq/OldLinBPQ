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


#include "BPQMail.h"

int LastVer[4] = {0, 0, 0, 0};					// In case we need to do somthing the first time a version is run

HWND MainWnd;
HWND hWndSess;
RECT MainRect;
HMENU hActionMenu;
static HMENU hMenu;
HMENU hDisMenu;									// Disconnect Menu Handle
HMENU hFWDMenu;									// Forward Menu Handle

int SessX, SessY, SessWidth;					// Params for Session Window

char szBuff[80];

#define MaxSockets 64

ConnectionInfo Connections[MaxSockets+1];

struct SEM ChatSemaphore = {0, 0};
struct SEM AllocSemaphore = {0, 0};
struct SEM ConSemaphore = {0, 0};
struct SEM Semaphore = {0, 0};
struct SEM OutputSEM = {0, 0};

struct UserInfo ** UserRecPtr=NULL;
int NumberofUsers=0;

struct UserInfo * BBSChain = NULL;					// Chain of users that are BBSes

struct MsgInfo ** MsgHddrPtr=NULL;
int NumberofMessages=0;

int FirstMessageIndextoForward = 0;					// Lowest Message wirh a forward bit set - limits search

BIDRec ** BIDRecPtr=NULL;
int NumberofBIDs=0;

BIDRec ** TempBIDRecPtr=NULL;
int NumberofTempBIDs=0;

WPRec ** WPRecPtr=NULL;
int NumberofWPrecs=0;

char ** BadWords=NULL;
int NumberofBadWords=0;
char * BadFile = NULL;

int LatestMsg = 0;
struct SEM MsgNoSemaphore = {0, 0};					// For locking updates to LatestMsg
int HighestBBSNumber = 0;

int MaxMsgno = 60000;
int BidLifetime = 60;
int MaxAge = 30;
int MaintInterval = 24;
int MaintTime = 0;

int UserLifetime = 0;

BOOL cfgMinToTray;

BOOL DisconnectOnClose=FALSE;

char PasswordMsg[100]="Password:";

char cfgHOSTPROMPT[100];

char cfgCTEXT[100];

char cfgLOCALECHO[100];

char AttemptsMsg[] = "Too many attempts - Disconnected\r\r";
char disMsg[] = "Disconnected by SYSOP\r\r";

char LoginMsg[]="user:";

char BlankCall[]="         ";


ULONG BBSApplMask;
ULONG ChatApplMask;

int BBSApplNum=0;
int ChatApplNum=0;

//int	StartStream=0;
int	NumberofStreams=0;
int MaxStreams=0;

char BBSSID[]="[%s%d.%d.%d.%d-%s%s%s%sIHJM$]\r";

char ChatSID[]="[BPQChatServer-%d.%d.%d.%d]\r";

char NewUserPrompt[100]="Please enter your Name\r>\r";

char * WelcomeMsg = NULL;
char * NewWelcomeMsg = NULL;
char * ExpertWelcomeMsg = NULL;

char * Prompt = NULL;
char * NewPrompt = NULL;
char * ExpertPrompt = NULL;

char BBSName[100] = "NOCALL";
char SYSOPCall[50];

char MailForText[100];

char HRoute[100];
char AMPRDomain[100];
BOOL SendAMPRDirect = 0;

char SignoffMsg[100];

char AbortedMsg[100]="\rOutput aborted\r";

char UserDatabaseName[MAX_PATH] = "BPQBBSUsers.dat";
char UserDatabasePath[MAX_PATH];

char MsgDatabasePath[MAX_PATH];
char MsgDatabaseName[MAX_PATH] = "DIRMES.SYS";

char BIDDatabasePath[MAX_PATH];
char BIDDatabaseName[MAX_PATH] = "WFBID.SYS";

char WPDatabasePath[MAX_PATH];
char WPDatabaseName[MAX_PATH] = "WP.SYS";

char BadWordsPath[MAX_PATH];
char BadWordsName[MAX_PATH] = "BADWORDS.SYS";

char NTSAliasesPath[MAX_PATH];
char NTSAliasesName[MAX_PATH] = "INTRCPT.APS";

char ConfigName[250];
char ChatConfigName[250];

BOOL UsingingRegConfig = FALSE;


char BaseDir[MAX_PATH];
char BaseDirRaw[MAX_PATH];			// As set in registry - may contain %NAME%
char ProperBaseDir[MAX_PATH];		// BPQ Directory/BPQMailChat


char MailDir[MAX_PATH];

char RlineVer[50];

BOOL KISSOnly = FALSE;

BOOL EnableUI = FALSE;
BOOL RefuseBulls = FALSE;
BOOL SendSYStoSYSOPCall = FALSE;
BOOL SendBBStoSYSOPCall = FALSE;
BOOL DontHoldNewUsers = FALSE;
BOOL ForwardToMe = FALSE;

BOOL DontNeedHomeBBS = FALSE;

// Send WP Params

BOOL SendWP;
char SendWPVIA[81];
char SendWPTO[11];
int SendWPType;

int SMTPMsgs;

int MailForInterval = 0;

char zeros[NBMASK];						// For forward bitmask tests

time_t MaintClock;						// Time to run housekeeping

struct MsgInfo * MsgnotoMsg[100000];	// Message Number to Message Slot List.

// Filter Params

char ** RejFrom;					// Reject on FROM Call
char ** RejTo;						// Reject on TO Call
char ** RejAt;						// Reject on AT Call

char ** HoldFrom;					// Hold on FROM Call
char ** HoldTo;						// Hold on TO Call
char ** HoldAt;						// Hold on AT Call

