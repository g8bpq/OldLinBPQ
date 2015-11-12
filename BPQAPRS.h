
#define OurSetItemText(hwndLV, i, iSubItem_, pszText_) \
{ LV_ITEM _ms_lvi;\
  _ms_lvi.iSubItem = iSubItem_;\
  _ms_lvi.pszText = pszText_;\
  SNDMSG((hwndLV), LVM_SETITEMTEXT, (WPARAM)i, (LPARAM)(LV_ITEM FAR *)&_ms_lvi);\
}

#define TRACKPOINTS 100

struct SORTLIST
{
	char Callsign[12];
	struct STATIONRECORD * Rec;

} SortList;

struct STATIONRECORD
{  
	struct STATIONRECORD * Next;
	char Callsign[12];
	char Path[120];
	char Status[256];
	char LastPacket[400];
	char LastWXPacket[256];
	int LastPort;
    double Lat;
    double Lon;
    double Course;
    double Speed;
    double Heading;
    double LatIncr;
    double LongIncr;
    double LastCourse;
    double LastSpeed;
    double Distance;
    double Bearing;
	double LatTrack[TRACKPOINTS];	// Cyclic Tracklog
	double LonTrack[TRACKPOINTS];
	time_t TrackTime[TRACKPOINTS];
	int Trackptr;					// Next record in Tracklog
	BOOL Moved;						// Moved since last drawn
    time_t TimeAdded;
    time_t TimeLastUpdated;
	UCHAR Symbol;
	int iconRow;
	int iconCol;					// Symbol Pointer
	char IconOverlay;
	int DispX;						// Position in display buffer
	int DispY;
	int Index;						// List Box Index
	BOOL NoTracks;					// Suppress displaying track
	COLORREF TrackColour;
	char ObjState;					// Live/Killed flag. If zero, not an object
	char LastRXSeq[6];				// Seq from last received message (used for Reply-Ack system)
	BOOL SimpleNumericSeq;			// Station treats seq as a number, not a text field
	struct STATIONRECORD * Object;	// Set if last record from station was an object
    time_t TimeLastTracked;			// Time of last trackpoint

} StationRecord;

struct OSMQUEUE
{
	struct OSMQUEUE * Next;
	int	Zoom;
	int x;
	int y;
};

struct APRSMESSAGE
{
	struct APRSMESSAGE * Next;
	struct STATIONRECORD * ToStation;	// Set on messages we send
	char FromCall[12];
	char ToCall[12];
	char Text[104];
	char Seq[8];
	BOOL Acked;
	int Retries;
	int RetryTimer;
	int Port;
	char Time[6];
	BOOL Cancelled;
};

struct APRSConnectionInfo			// Used for Web Server for thread-specific stuff
{
	struct STATIONRECORD * SelCall;	// Station Record for individual statond display
	HANDLE hPipe;
	SOCKET sock;
	char Callsign[12];
	int WindDirn, WindSpeed, WindGust, Temp, RainLastHour, RainLastDay, RainToday, Humidity, Pressure; //WX Fields
};

#define BPQBASE     WM_USER
//
//	Port monitoring flags use BPQBASE -> BPQBASE+16

#define BPQMTX	      BPQBASE+40
#define BPQMCOM	      BPQBASE+41
//#define BPQCOPY       BPQBASE+42
