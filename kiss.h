

#define MAXBLOCK        512


typedef struct tagASYINFO
{
	HANDLE  idComDev ;
	BYTE    bPort;
	DWORD   dwBaudRate ;
	SOCKET	sock;			// for KISS over UDP/TCP
	BOOL	Connecting;		// Kiss over TCP
	BOOL	Connected;		// Kiss over TCP
	BOOL	Listening;		// Kiss over TCP
	BOOL	Alerted;		// Connect Fail Reported

	struct sockaddr_in destaddr;
	struct PORTCONTROL * Portvector;
	UCHAR	RXMSG[512];				// Msg being built
	UCHAR	RXBUFFER[MAXBLOCK];		// Raw chars from Comms
	int		RXBCOUNT;				// chars in RXBUFFER
	UCHAR * RXBPTR;					// get pointer for RXBUFFER (put ptr is RXBCOUNT) 
	UCHAR * RXMPTR;					// put pointer for RXMSG
	BOOL	MSGREADY;				// Complete msg in RXMSG
	BOOL	ESCFLAG;				// FESC received
 
} ASYINFO, *NPASYINFO ;

NPASYINFO KISSInfo[33] = {0};


#define _fmemset   memset
#define _fmemmove  memmove

// function prototypes (private)

NPASYINFO CreateKISSINFO( int port, int speed );


BOOL DestroyKISSINFO(NPASYINFO npKISSINFO) ;
int ReadCommBlock(NPASYINFO npKISSINFO, char * lpszBlock, int nMaxLength);
static BOOL WriteCommBlock(NPASYINFO npKISSINFO, char * lpByte, DWORD dwBytesToWrite);
HANDLE OpenConnection(struct PORTCONTROL * PortVector, int port);
BOOL SetupConnection(NPASYINFO npKISSINFO);
BOOL CloseConnection(NPASYINFO npKISSINFO);

//---------------------------------------------------------------------------
//  End of File: tty.h
//---------------------------------------------------------------------------
