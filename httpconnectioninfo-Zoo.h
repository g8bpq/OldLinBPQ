//
//	HTTP Session Control. Used In Kernel HTTPCode, BBSHTMLConfig
//	and ChatHTMLConfig

struct HTTPConnectionInfo		// Used for Web Server for thread-specific stuff
{
	struct HTTPConnectionInfo * Next;
	struct STATIONRECORD * SelCall;	// Station Record for individual statond display
	char Callsign[12];
	int WindDirn, WindSpeed, WindGust, Temp, RainLastHour, RainLastDay, RainToday, Humidity, Pressure; //WX Fields
	char * ScreenLines[100];	// Screen Image for Teminal access mode - max 100 lines (cyclic buffer)
	int ScreenLineLen[100];		// Length of each lime
	int LastLine;				// Pointer to last line of data
	BOOL PartLine;				// Last line does not have CR on end
	char HTTPCall[10];			// Call of HTTP user
	BOOL Changed;				// Changed since last poll. If set, reply immediately, else set timer and wait
	SOCKET sock;				// Socket for pending send
	int ResponseTimer;			// Timer for delayed response
	int KillTimer;				// Clean up timer (no activity timeout)
	int Stream;					// BPQ Stream Number
	char Key[20];				// Session Key
	BOOL Connected;
	// Use by Mail Module
#ifdef MAIL
	struct UserInfo * User;		// Selected User
	struct MsgInfo * Msg;		// Selected Message
	WPRec * WP;					// Selected WP record
#else
	VOID * User;				// Selected User
	VOID * Msg;					// Selected Message
	VOID * WP;					// Selected WP record
#endif
	struct UserRec * USER;		// Telnet Server USER record
	int WebMailSkip;			// Number to skip at start of list (for paging)
	char WebMailTypes[4];		// Types To List
	time_t WebMailLastUsed;
};
