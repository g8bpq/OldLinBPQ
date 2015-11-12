#define GTK_ENABLE_BROKEN
#define _CRT_SECURE_NO_DEPRECATE

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
GdkPixbuf *create_pixbuf(const gchar * filename)
{
   GdkPixbuf *pixbuf;
   GError *error = NULL;
   pixbuf = gdk_pixbuf_new_from_file(filename, &error);
   if(!pixbuf) {
      fprintf(stderr, "%s\n", error->message);
      g_error_free(error);
   }

   return pixbuf;
}

#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define SOCKET int
#define closesocket close
typedef gint32   COLORREF;
#define RGB(r,g,b)          ((COLORREF)(((guint8)(r)|((guint16)((guint8)(g))<<8))|(((guint32)(guint8)(b))<<16)))

gchar *fontname;
gint vhandle;
char Host[5][100];
char Port[5][10];
char UserName[5][80];
char Password[5][80];
char Path[5][100];
char Test1[5][100];
char Test2[5][100];
char Pass[5][100];
char path[5][100];

char HN[9][10] = {"Host1", "Host2", "Host3", "Host4", "Host5","Host6", "Host7", "Host8"};
char PN[9][10] = {"Port1", "Port2", "Port3", "Port4", "Port5", "Port6", "Port7", "Port8"};
char PASSN[9][10] = {"Pass1", "Pass2", "Pass3", "Pass4", "Pass5", "Pass6", "Pass7", "Pass8"};
char UN[9][10] = {"User1", "User2", "User3", "User4", "User5", "User6", "User7", "User8"};
char pn[5][6] = {"Path"};
int CurrentHost = 0;
char VersionString[80] = "1.0.0.47a BPQ";

char DisMsg[] = "*** Disconnected\r";

int PortMask=65535;
int mtxparam=1;
int MCOM=1;
int Split;
int x, y;
int vhandle;
int Bells = FALSE;
int StripLF = FALSE;
int LogMonitor = FALSE;
int LogOutput = FALSE;
int SendDisconnected = TRUE;
int MonNODES = FALSE;
int MONColour = TRUE;
int ChatMode = FALSE;
int MonPorts = 1;
int muionly = 1;
gchar * font;
char Font[50];
int position;
int left = 100, top = 100, right = 500, bottom = 500;
int height;
int width;
char Position[50];
gint gx, gy;
gint xx, yy;
gint xxx, yyy;
char outputmon[50];
char monitormon [50];
int Connecting = FALSE;
int Disconnecting = FALSE;
int Connected = FALSE;
int SocketActive = FALSE;
int monitormon1;

char Title[80];

int SlowTimer;

void ReadConfig();
void SendTraceOptions();

int TCPConnect(char * Host, char * Port);
void WritetoOutputWindow(const char * Text, int Len);
void WritetoMonWindow(char * Text, int Len);
int Telnet_Connected(SOCKET sock, int Error);
int SendMsg(const char * msg, int len);


COLORREF Colours[256] = {0,
		RGB(0,0,0), RGB(0,0,128), RGB(0,0,192), RGB(0,0,255),				// 1 - 4
		RGB(0,64,0), RGB(0,64,128), RGB(0,64,192), RGB(0,64,255),			// 5 - 8
		RGB(0,128,0), RGB(0,128,128), RGB(0,128,192), RGB(0,128,255),		// 9 - 12
		RGB(0,192,0), RGB(0,192,128), RGB(0,192,192), RGB(0,192,255),		// 13 - 16
		RGB(0,255,0), RGB(0,255,128), RGB(0,255,192), RGB(0,255,255),		// 17 - 20

		RGB(64,0,0), RGB(64,0,128), RGB(64,0,192), RGB(0,0,255),				// 21
		RGB(64,64,0), RGB(64,64,128), RGB(64,64,192), RGB(64,64,255),
		RGB(64,128,0), RGB(64,128,128), RGB(64,128,192), RGB(64,128,255),
		RGB(64,192,0), RGB(64,192,128), RGB(64,192,192), RGB(64,192,255),
		RGB(64,255,0), RGB(64,255,128), RGB(64,255,192), RGB(64,255,255),

		RGB(128,0,0), RGB(128,0,128), RGB(128,0,192), RGB(128,0,255),				// 41
		RGB(128,64,0), RGB(128,64,128), RGB(128,64,192), RGB(128,64,255),
		RGB(128,128,0), RGB(128,128,128), RGB(128,128,192), RGB(128,128,255),
		RGB(128,192,0), RGB(128,192,128), RGB(128,192,192), RGB(128,192,255),
		RGB(128,255,0), RGB(128,255,128), RGB(128,255,192), RGB(128,255,255),

		RGB(192,0,0), RGB(192,0,128), RGB(192,0,192), RGB(192,0,255),				// 61
		RGB(192,64,0), RGB(192,64,128), RGB(192,64,192), RGB(192,64,255),
		RGB(192,128,0), RGB(192,128,128), RGB(192,128,192), RGB(192,128,255),
		RGB(192,192,0), RGB(192,192,128), RGB(192,192,192), RGB(192,192,255),
		RGB(192,255,0), RGB(192,255,128), RGB(192,255,192), RGB(192,2552,255),

		RGB(255,0,0), RGB(255,0,128), RGB(255,0,192), RGB(255,0,255),				// 81
		RGB(255,64,0), RGB(255,64,128), RGB(255,64,192), RGB(255,64,255),
		RGB(255,128,0), RGB(255,128,128), RGB(255,128,192), RGB(255,128,255),
		RGB(255,192,0), RGB(255,192,128), RGB(255,192,192), RGB(255,192,255),
		RGB(255,255,0), RGB(255,255,128), RGB(255,255,192), RGB(255,2552,255)
};


SOCKET RecvSocket;
SOCKET sock;

GtkWidget *dialog;
GtkWidget *window;
GtkWidget *box1;
GtkWidget *box2;
GtkWidget *box3;
GtkWidget *box10;
GtkWidget *hbox;
GtkWidget *button;
GtkWidget *check;
GtkWidget *separator;
GtkWidget *table;
GtkWidget *vscrollbar;
GtkWidget *vscrollbar2;
GtkTextBuffer *text;
GtkTextBuffer *text2;
GtkTextBuffer *text3;
GtkWidget *entry;
GtkWidget *vpaned;
GtkWidget *frame1;
GtkWidget *frame2;
GIOChannel *RecvChannel;
GtkWidget *menubar;
GtkWidget *view;
GtkWidget *scrolledwin;
GtkWidget *view2;
GtkWidget *scrolledwin2;
GtkWidget *scrolledwin3;
GtkWidget *montx;
GtkWidget *monsup;
GtkWidget *monnode;
GtkWidget *encol;
GtkWidget *mui;
GtkWidget *addpor;
GtkWidget *menubar;
GtkWidget *conmenu, *conn_item, *Conn[8], *Conn2, *Conn3, *Conn4, *Conn5, *Conn6, *Conn7, *Conn8;
GtkWidget *discmenu, *dis_item, sid_item;
GtkWidget *cfgmenu, *tcp_item, *font_item, *strip_item, *logmon_item, *logout_item, *cfg_item, *chat_term, *Cfg[8], *Cfg2, *Cfg3, *Cfg4, *Cfg5, *Cfg6, *Cfg7, *Cfg8;
GtkWidget *monmenu, *mon_item, *mon[32];
GtkWidget *tcpmenu;
GtkWidget *enbel, *enbel_item;
GtkWidget *propmenu,*propitem,*Prop;
GtkWidget *text_view, *font_select_dlg;
GtkWidget *font_select_dlg_ok, *font_select_dlg_apply, *font_select_dlg_cancel;
gchar *fontname;
GtkTextTag *rtag, *btag, *tag[256], *tagm[256];

void EnableDisconnectMenu();
void DisableConnectMenu();
void EnableConnectMenu();
void DisableDisconnectMenu();
void get_fontname(GtkWidget *, gpointer);
void apply_fontname(GtkWidget *, gpointer);
void open_font_select_dlg(GtkWidget *, gpointer);
void font_select_dlg_exit(GtkWidget *, gpointer);
void close_application(GtkWidget *widget, gpointer   data)
{
       gtk_main_quit ();
       return;
}

void enter_callback( GtkWidget *widget,
                     GtkWidget *entry )
{
	const gchar *entry_text;
	entry_text = gtk_entry_get_text (GTK_ENTRY (entry));

	if (!Connected && ! Connecting)
	{
		TCPConnect(Host[CurrentHost], Port[CurrentHost]);
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		return ;
	}

	SendMsg(entry_text, strlen(entry_text));
	SendMsg("\r", 1);
	WritetoOutputWindow(entry_text, strlen(entry_text));
	WritetoOutputWindow("\r ", 1);
	gtk_entry_set_text (GTK_ENTRY (entry), "");
}

static void Disconnect(GtkWidget *w, gpointer data);
static void Toggled(GtkWidget *w, int * data )
{
      int NewVal = gtk_check_menu_item_get_active((GtkCheckMenuItem *)w);
      *(data) = NewVal;

      SendTraceOptions();

      return;
}

static void Configure(GtkWidget *w, gpointer data)
{
	GtkWidget * dialog = gtk_dialog_new_with_buttons( "Configuration",
                                              GTK_WINDOW(window),
                                              GTK_DIALOG_MODAL,
                                              GTK_STOCK_OK, 1,
                                              GTK_STOCK_CANCEL,  2,
                                              NULL );

	GtkWidget *entry1;
	GtkWidget *entry2;
	GtkWidget *entry3;
	GtkWidget *entry4;

	GtkWidget *label, *content_area;
	GtkWidget *label2, *label3, *label4;

	int HostNum = (int)data;
	const gchar *entry_text;
	gint result;
	char Key[10];

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	label = gtk_label_new ("Host");
	label2 = gtk_label_new ("Port");
	label3 = gtk_label_new ("Username");
	label4 = gtk_label_new ("Password");

	/* Add the label, and show everything we've added to the dialog. */

	gtk_container_add (GTK_CONTAINER (content_area), label);

	entry1 = gtk_entry_new();
	gtk_entry_set_max_length (GTK_ENTRY (entry1), 100);
	gtk_entry_set_text (GTK_ENTRY (entry1), &Host[HostNum][0]);

	gtk_container_add (GTK_CONTAINER (content_area), entry1);

	gtk_container_add (GTK_CONTAINER (content_area), label2);
	entry2 = gtk_entry_new();
	gtk_entry_set_max_length (GTK_ENTRY (entry2), 10);
	gtk_entry_set_text (GTK_ENTRY (entry2), &Port[HostNum][0]);

	gtk_container_add (GTK_CONTAINER (content_area), entry2);

	gtk_container_add (GTK_CONTAINER (content_area), label3);

   	entry3 = gtk_entry_new();
	gtk_entry_set_max_length (GTK_ENTRY (entry3), 100);
	gtk_entry_set_text (GTK_ENTRY (entry3), &UserName[HostNum][0]);
	gtk_container_add (GTK_CONTAINER (content_area), entry3);

	gtk_container_add (GTK_CONTAINER (content_area), label4);

	entry4 = gtk_entry_new();
	gtk_entry_set_max_length (GTK_ENTRY (entry4), 100);
	gtk_entry_set_text (GTK_ENTRY (entry4), &Password[HostNum][0]);
	gtk_container_add (GTK_CONTAINER (content_area), entry4);

	gtk_widget_show_all (dialog);

	result = gtk_dialog_run (GTK_DIALOG (dialog));

	if (result == 1)
	{
		GKeyFile * KF;
		gchar * Value;
		GError *error = NULL;
		gsize length;
		FILE *outfile;
		char path[PATH_MAX];
		char *fname = ".BPQTermTCP.ini";
		snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"), fname);

		entry_text = gtk_entry_get_text (GTK_ENTRY (entry1));
		strcpy(&Host[HostNum][0], entry_text);

		entry_text = gtk_entry_get_text (GTK_ENTRY (entry2));
		strcpy(&Port[HostNum][0], entry_text);

		entry_text = gtk_entry_get_text (GTK_ENTRY (entry3));
		strcpy(&UserName[HostNum][0], entry_text);

		entry_text = gtk_entry_get_text (GTK_ENTRY (entry4));
		strcpy(&Password[HostNum][0], entry_text);

		KF = g_key_file_new();
		g_key_file_load_from_file(KF, path, 0, NULL);

		sprintf(Key, "Host%d", HostNum + 1);
		g_key_file_set_string(KF, "Session 1", Key, &Host[HostNum][0]);

		sprintf(Key, "Port%d", HostNum + 1);
		g_key_file_set_string(KF, "Session 1", Key, &Port[HostNum][0]);

		sprintf(Key, "User%d", HostNum + 1);
		g_key_file_set_string(KF, "Session 1", Key, &UserName[HostNum][0]);

		sprintf(Key, "Pass%d", HostNum + 1);
		g_key_file_set_string(KF, "Session 1", Key, &Password[HostNum][0]);


		Value = g_key_file_to_data(KF, &length, &error);

		outfile = fopen ( path, "w");
		fputs(Value, outfile);
		fclose(outfile);

		g_free(Value);

		g_key_file_free(KF);
		ReadConfig();
    }

	gtk_widget_destroy (dialog);
}

static void Connect( GtkWidget *w, gpointer  data )
{
	CurrentHost = (int)data;
	TCPConnect(Host[CurrentHost], Port[CurrentHost]);
}


    // Port Line Callback. data Param is Port Number
static void PToggled( GtkWidget *w, int  data )
{
      // Create Port Mask bit from Port Number
      int Mask = 1 << data;

      // Get current state of Item
      int NewVal = gtk_check_menu_item_get_active((GtkCheckMenuItem *)w);
      PortMask &= ~Mask;            // Clear portmask bit for this port

      // Shift the new bit to the right place in the mask
      NewVal = NewVal << data;

      // OR into Mask
      PortMask |= NewVal;

      SendTraceOptions();

      return;

}

static void AddPortItem( GtkWidget *w, int * data )
{
      char Port[10];
      sprintf(Port, "Port %d", MonPorts + 1);
      mon[MonPorts] = gtk_check_menu_item_new_with_label (Port);
      gtk_check_menu_item_set_active((GtkCheckMenuItem *)mon[MonPorts], (PortMask >> MonPorts) & MonPorts);

      // Set Callback to PToggled. Parameter is Port Number
      g_signal_connect (mon[MonPorts], "toggled", G_CALLBACK (PToggled), (void *) MonPorts);
      gtk_menu_shell_append ((GtkMenuShell *)monmenu, mon[MonPorts]);

      MonPorts++;

      gtk_widget_show_all (menubar);

       return;
}

GtkWidget *get_menubar_menu(GtkWidget  *window)
{
	int i;

	menubar = gtk_menu_bar_new();
	conmenu = gtk_menu_new();
	cfgmenu = gtk_menu_new();
    discmenu = gtk_menu_new();
    monmenu = gtk_menu_new();
    montx = gtk_menu_new();
    monsup = gtk_menu_new();
    monnode = gtk_menu_new();
    encol = gtk_menu_new();
    addpor = gtk_menu_new();
    tcpmenu = gtk_menu_new();
    enbel = gtk_menu_new();
    mui = gtk_menu_new();
    propmenu = gtk_menu_new();

    /* Create the menu items */

    for (i = 0; i < 8; i++)
	{
		Conn[i] = gtk_check_menu_item_new_with_label (Host[i]);
		gtk_menu_shell_append (GTK_MENU_SHELL (conmenu), Conn[i]);
		gtk_check_menu_item_set_active((GtkCheckMenuItem *) Conn[i], CurrentHost == i);
        g_signal_connect (Conn[i], ("toggled"), G_CALLBACK (Toggled), (void *) &CurrentHost);

		Cfg[i] = gtk_menu_item_new_with_label (Host[i]);
	    gtk_menu_shell_append (GTK_MENU_SHELL (tcpmenu), Cfg[i]);


		/* Attach the callback functions to the activate signal */

		g_signal_connect (Conn[i], "activate", G_CALLBACK (Connect), (void *) i);
 		g_signal_connect (Cfg[i], "activate", G_CALLBACK (Configure), (void *) i);

	}

	conn_item = gtk_menu_item_new_with_label ("Connect");
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (conn_item), conmenu);

    dis_item = gtk_menu_item_new_with_label ("Disconnect");
    g_signal_connect (dis_item, "activate", G_CALLBACK (Disconnect), 0);
	gtk_widget_set_sensitive(dis_item, TRUE);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (dis_item), discmenu);

    cfg_item = gtk_menu_item_new_with_label ("Setup");
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (cfg_item), cfgmenu);

        enbel = gtk_check_menu_item_new_with_label ("Enable Bells");
        gtk_check_menu_item_set_active((GtkCheckMenuItem *)enbel, Bells);
        g_signal_connect (enbel, "toggled", G_CALLBACK(Toggled), (void *) &Bells);

        logmon_item = gtk_menu_item_new_with_label ("Log Monitor");
        logout_item = gtk_menu_item_new_with_label ("Log Output");

        chat_term = gtk_check_menu_item_new_with_label ("Chat Terminal Mode");
        gtk_check_menu_item_set_active((GtkCheckMenuItem *) chat_term, ChatMode);
        g_signal_connect (chat_term, ("toggled"), G_CALLBACK (Toggled), (void *) &ChatMode);

     propitem = gtk_menu_item_new_with_label ("Properties");
     g_signal_connect(propitem, "activate", G_CALLBACK (open_font_select_dlg), (void*) &font);
     //gtk_menu_item_set_submenu (GTK_MENU_ITEM (propitem), propmenu);


    tcp_item = gtk_menu_item_new_with_label ("TCP Hosts");
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (tcp_item), tcpmenu);

    mon_item = gtk_menu_item_new_with_label ("Monitor");
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (mon_item), monmenu);

	  montx = gtk_check_menu_item_new_with_label ("Monitor TX");
      gtk_check_menu_item_set_active((GtkCheckMenuItem *)montx, mtxparam);
      g_signal_connect (montx, "toggled", G_CALLBACK (Toggled), (void *) &mtxparam);

      monsup = gtk_check_menu_item_new_with_label ("Monitor Supervisor");
      gtk_check_menu_item_set_active((GtkCheckMenuItem *)monsup, MCOM);
      g_signal_connect (monsup, "toggled", G_CALLBACK (Toggled), (void *) &MCOM);

	  monnode = gtk_check_menu_item_new_with_label ("Monitor Nodes");
      gtk_check_menu_item_set_active((GtkCheckMenuItem *)monnode, MonNODES);
      g_signal_connect (monnode, "toggled", G_CALLBACK (Toggled), (void *) &MonNODES);

      mui = gtk_check_menu_item_new_with_label ("Monitor UI Only");
      gtk_check_menu_item_set_active((GtkCheckMenuItem *)mui, muionly);
      g_signal_connect (mui, "toggled", G_CALLBACK (Toggled), (void *) &muionly);

      encol = gtk_check_menu_item_new_with_label ("Enable Colour");
      gtk_check_menu_item_set_active((GtkCheckMenuItem *)encol, MONColour);
      g_signal_connect (encol, "toggled", G_CALLBACK (Toggled), (void *) &MONColour);

      addpor = gtk_menu_item_new_with_label ("Add Port");
      g_signal_connect (addpor, "activate", G_CALLBACK (AddPortItem), (void *) 0);


    /* Add them to the menu */

    gtk_menu_shell_append ((GtkMenuShell *)menubar, conn_item);
	gtk_menu_shell_append ((GtkMenuShell *)menubar, dis_item);
	gtk_menu_shell_append ((GtkMenuShell *)menubar, cfg_item);
	gtk_menu_shell_append ((GtkMenuShell *)menubar, mon_item);
    gtk_menu_shell_append ((GtkMenuShell *)monmenu, montx);
	gtk_menu_shell_append ((GtkMenuShell *)monmenu, monsup);
	gtk_menu_shell_append ((GtkMenuShell *)monmenu, monnode);
	gtk_menu_shell_append ((GtkMenuShell *)monmenu, mui);
	gtk_menu_shell_append ((GtkMenuShell *)monmenu, encol);
	gtk_menu_shell_append ((GtkMenuShell *)monmenu, addpor);
    gtk_menu_shell_append ((GtkMenuShell *)cfgmenu, tcp_item);
    gtk_menu_shell_append ((GtkMenuShell *)cfgmenu, propitem);
    gtk_menu_shell_append ((GtkMenuShell *)cfgmenu, enbel);
    gtk_menu_shell_append ((GtkMenuShell *)cfgmenu, logmon_item);
    gtk_menu_shell_append ((GtkMenuShell *)cfgmenu, logout_item);
    gtk_menu_shell_append ((GtkMenuShell *)cfgmenu, chat_term);

            for (i = 0; i < MonPorts; i++)

      {

            char Port[10];
            sprintf(Port, "Port %d", i + 1);
            mon[i] = gtk_check_menu_item_new_with_label (Port);

            // Set the Checked flag from the corresponding bit of PortMask
            gtk_check_menu_item_set_active((GtkCheckMenuItem *)mon[i], (PortMask >> i) & 1);

            // Call PToggled() when menu is selected. Data to PToggled is Port Number
            g_signal_connect (mon[i], "toggled", G_CALLBACK (PToggled), (void *) i);
            gtk_menu_shell_append ((GtkMenuShell *)monmenu, mon[i]);

      }

 	gtk_widget_show_all (menubar);

    SendTraceOptions();

	return menubar;

}

gint ScrollTimer(gpointer data)
{
	GtkTextIter iter;

	gtk_text_buffer_get_end_iter(text, &iter);
	gtk_text_view_scroll_to_iter ((GtkTextView *)view, &iter, 0.0, FALSE, 0.0, 0.0);

    return FALSE;
}
gint ScrollTimer2(gpointer data)
{
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(text2, &iter);
	gtk_text_view_scroll_to_iter ((GtkTextView *)view2, &iter, 0.0, FALSE, 0.0, 0.0);

	return FALSE;
}

static fd_set readfs;
static fd_set writefs;
static fd_set errorfs;
static struct timeval timeout;

int MonData = FALSE;


//int Connecting = FALSE;
//int Disconnecting = FALSE;
//int Connected = FALSE;
//int SocketActive = FALSE;

int ProcessReceivedData();

gint PollTimer(gpointer data)
{
	FD_ZERO(&readfs);

	if (Connecting ||Connected)
		FD_SET(sock,&errorfs);
	else
		return TRUE;

	if (Connected) FD_SET(sock,&readfs);

	FD_ZERO(&writefs);

	if (Connecting) FD_SET(sock,&writefs);	// Need notification of Connect

	FD_ZERO(&errorfs);

	if (Connecting ||Connected) FD_SET(sock,&errorfs);

	if (select(sock + 1, &readfs, &writefs, &errorfs, &timeout) > 0)
	{
		//	See what happened

		if (FD_ISSET(sock, &readfs))
		{
			// data available

			ProcessReceivedData();

		}

		if (FD_ISSET(sock, &writefs))
		{
			//	Connect success

			Connecting = FALSE;
			Connected = TRUE;

		}

		if (FD_ISSET(sock, &errorfs))
		{
			//	if connecting, then failed, if connected then has just disconnected

			if (Connecting)
			{
				// Falied

				Connecting = FALSE;
				Connected = FALSE;
			}
			else
			{
				if (SocketActive)
					closesocket(sock);
				else
					return TRUE;

				sprintf(Title,"TermTCP Version %s - Disconnected", VersionString);
				gtk_window_set_title (GTK_WINDOW (window), Title);
				DisableDisconnectMenu();
				EnableConnectMenu();

				WritetoOutputWindow(DisMsg, strlen(DisMsg));
				SocketActive = FALSE;
				Connected = FALSE;
				Disconnecting = FALSE;
				MonData = FALSE;
				return TRUE;

			}
		}
	}

	if (!Connected)
		return TRUE;

	if (!ChatMode)
		return TRUE;
		
	SlowTimer++;
	
	if (SlowTimer > 5 * 60 * 9)				// About 9 mins
	{
		SlowTimer = 0;
		SendMsg("\0", 1);
	}

	return TRUE;
}

static GtkWidget *create_monitor ( void )
{
    view = gtk_text_view_new ();
	text = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);

	scrolledwin = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwin), 3);
	gtk_widget_set_size_request(scrolledwin, 500, 100);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER (scrolledwin), view);

    gtk_widget_show(scrolledwin);
    return scrolledwin;

}

static GtkWidget *create_output ( void )
{
    view2 = gtk_text_view_new ();
	text2 = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view2));
    gtk_text_view_set_editable (GTK_TEXT_VIEW(view2), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW(view2), FALSE);
 	scrolledwin2 = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwin2), 3);
	gtk_widget_set_size_request(scrolledwin2, 500, 100);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin2),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin2), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(scrolledwin2), view2);

    gtk_widget_show(scrolledwin2);
    return scrolledwin2;
}

gint delete_event( GtkWidget *widget,
                   GdkEvent  *event,
                   gpointer   data )
{
    /* If you return FALSE in the "delete_event" signal handler,
     * GTK will emit the "destroy" signal. Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit?'
     * type dialogs. */

    g_print ("delete event occurred\n");
    gtk_window_get_size(GTK_WINDOW(widget), &width, &height);
	gtk_window_get_position(GTK_WINDOW(widget), &x, &y);
	vhandle = gtk_paned_get_position((GtkPaned *)vpaned);
	printf("%d %d %d %d %d\n", x, y, width, height, vhandle);
    //printf("%s\n", Font);
    //gtk_widget_get_style(GtkWidget* window);
    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete_event". */

    return(FALSE);
}


int main(int argc, char *argv[])
{

	int i;

    PangoFontDescription *font_desc;

	gtk_init (&argc, &argv);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
                        GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

   	ReadConfig();

	gtk_window_set_default_size(GTK_WINDOW (window), width, height);
	gtk_widget_set_uposition(GTK_WIDGET(window),gx, gy);
	gtk_window_set_resizable  (GTK_WINDOW (window), TRUE);
	g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (close_application), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "TermTCP");
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_icon(GTK_WINDOW(window), create_pixbuf("/usr/share/pixmaps/bpqicon.png"));
    //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    //gtk_window_get_frame_dimensions(GTK_WINDOW(window),&left,&top,&right,&bottom);


    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                        GTK_SIGNAL_FUNC (delete_event), NULL);

	// Create a box for the menu

	box1 = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), box1);
	box10 = gtk_vbox_new (FALSE, 0);

	menubar = get_menubar_menu (window);

    gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 1);
	gtk_container_add (GTK_CONTAINER (box1), box10);
    gtk_widget_show (window);

     vpaned = gtk_vpaned_new ();
     gtk_container_add (GTK_CONTAINER (box10), vpaned);
     gtk_paned_set_position(GTK_PANED(vpaned), vhandle);
     gtk_widget_show (vpaned);

    /* Now create the contents of the two halves of the window */

    frame1 = create_monitor ();
    gtk_paned_add1 (GTK_PANED (vpaned), frame1);
    gtk_widget_show (frame1);

    frame2 = create_output ();
    gtk_paned_add2 (GTK_PANED (vpaned), frame2);
    gtk_widget_show (frame2);

    /* Separator */
//	separator = gtk_hseparator_new ();
//	gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

	box2 = gtk_vbox_new  (FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER (box2), 1);
	gtk_box_pack_start (GTK_BOX (box10), box2, FALSE, FALSE, 0);

	// set up the text entry line

	entry = gtk_entry_new();
	//gtk_entry_new_with_buffer(text);
	gtk_entry_set_max_length (GTK_ENTRY (entry), width);
	gtk_entry_set_activates_default(GTK_ENTRY (entry), TRUE);
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (enter_callback), (gpointer) entry);
	gtk_box_pack_start (GTK_BOX (box2), entry, FALSE, FALSE, 0);
    gtk_widget_grab_focus(entry);

	gtk_widget_show_all (window);

	/* Change default font throughout the widget */

    font_desc=pango_font_description_from_string(Font);
    gtk_widget_modify_font (entry, font_desc);
	gtk_widget_modify_font (view, font_desc);
	gtk_widget_modify_font (view2, font_desc);
//  gtk_entry_new_with_buffer();
    //gtk_signal_connect(G_OBJECT(window), "configure-event",
    //                G_CALLBACK(frame_callback), NULL);

	rtag = gtk_text_buffer_create_tag (text, NULL, "foreground", "red", NULL);
	btag = gtk_text_buffer_create_tag (text, NULL, "foreground", "blue", NULL);

	for (i = 0; i < 100; i++)
	{
		tag[i] = gtk_text_buffer_create_tag (text2, NULL, "foreground", "red", NULL);
		tag[i]->values->appearance.fg_color.red = (Colours[i] & 0xff) << 8;
		tag[i]->values->appearance.fg_color.green = (Colours[i] & 0xff00);
		tag[i]->values->appearance.fg_color.blue = (Colours[i] & 0xff0000) >> 8;
	}

	for (i = 0; i < 100; i++)
	{
		tagm[i] = gtk_text_buffer_create_tag (text, NULL, "foreground", "red", NULL);
		tagm[i]->values->appearance.fg_color.red = (Colours[i] & 0xff) << 8;
		tagm[i]->values->appearance.fg_color.green = (Colours[i] & 0xff00);
		tagm[i]->values->appearance.fg_color.blue = (Colours[i] & 0xff0000) >> 8;
	}

	g_timeout_add (200, PollTimer, 0);

	gtk_main ();

	{

		GKeyFile * KF;
		gchar * Value;
		GError *error = NULL;
		gsize length;
		FILE *outfile;

        char path[PATH_MAX];
        char *fname = ".BPQTermTCP.ini";
        snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"), fname);

        KF = g_key_file_new();
		g_key_file_load_from_file(KF, path, 0, NULL);


        //printf("%d %d\n", width, height);

        g_key_file_set_integer(KF, "Session 1", "MTX", mtxparam);
		g_key_file_set_integer(KF, "Session 1", "MCOM", MCOM);
		g_key_file_set_integer(KF, "Session 1", "MonNODES", MonNODES);
		g_key_file_set_integer(KF, "Session 1", "ChatMode", ChatMode);
		g_key_file_set_integer(KF, "Session 1", "Bells", Bells);
        g_key_file_set_integer(KF, "Session 1", "CurrentHost", CurrentHost);
		g_key_file_set_integer(KF, "Session 1", "MONColour", MONColour);
        g_key_file_set_integer(KF, "Session 1", "MonPorts", MonPorts);
        g_key_file_set_integer(KF, "Session 1", "PortMask", PortMask);
        g_key_file_set_integer(KF, "Session 1", "MUIONLY", muionly);

        //sprintf(Font, "%s", Font);
        //g_key_file_set_string(KF, "Session 1", "Font", Font);


        sprintf(Position, "%d,%d,%d,%d,%d", x, y, width, height, vhandle);
        g_key_file_set_string(KF, "Session 1", "Position", Position);

        //sprintf(monitormon, "%d,%d", xx, yy);
        //g_key_file_set_string(KF, "Session 1", "Scrollwin", monitormon);
        Value = g_key_file_to_data(KF, &length, &error);

		outfile = fopen ( path, "w");
		fputs(Value, outfile);
		fclose(outfile);

		g_free(Value);

		g_key_file_free(KF);
	}

	return 0;
}


void open_font_select_dlg(GtkWidget *widget, gpointer data)
{
    font_select_dlg=gtk_font_selection_dialog_new( "Select Font");
    font_select_dlg_ok=GTK_FONT_SELECTION_DIALOG(font_select_dlg)->ok_button;
    font_select_dlg_apply=GTK_FONT_SELECTION_DIALOG(font_select_dlg)->apply_button;
    font_select_dlg_cancel=GTK_FONT_SELECTION_DIALOG(font_select_dlg)->cancel_button;
    g_signal_connect(G_OBJECT(font_select_dlg_ok), "clicked", G_CALLBACK(get_fontname), NULL);
    g_signal_connect(G_OBJECT(font_select_dlg_apply), "clicked", G_CALLBACK(apply_fontname), NULL);
    g_signal_connect(G_OBJECT(font_select_dlg_cancel), "clicked", G_CALLBACK(font_select_dlg_exit), NULL);
    gtk_widget_show_all(font_select_dlg);
}

void get_fontname(GtkWidget *widget, gpointer data)
{
    gchar *fontname;
    fontname=gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(font_select_dlg));
    PangoFontDescription *font_desc;
    font_desc=pango_font_description_from_string(Font);
    strcpy(Font, font_desc);
    gtk_widget_modify_font (entry, font_desc);
    gtk_widget_modify_font (view, font_desc);
    gtk_widget_modify_font (view2, font_desc);
    gtk_widget_modify_font (window, font_desc);
    pango_font_description_free (font_desc);

    GKeyFile * KF;
		gchar * Value;
		GError *error = NULL;
		gsize length;
		FILE *outfile;

        char path[PATH_MAX];
        char *fname = ".BPQTermTCP.ini";
        snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"), fname);

        KF = g_key_file_new();
		g_key_file_load_from_file(KF, path, 0, NULL);

        sprintf(Font, "%s", fontname);
        g_key_file_set_string(KF, "Session 1", "Font", Font);

        Value = g_key_file_to_data(KF, &length, &error);

		outfile = fopen ( path, "w");
		fputs(Value, outfile);
		fclose(outfile);

		g_free(Value);

		g_key_file_free(KF);
		gtk_widget_destroy(font_select_dlg);
//        gtk_entry_new_with_buffer();
    //return;
}


void apply_fontname(GtkWidget *widget, gpointer data)
{
    gchar *fontname;
    fontname=gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(font_select_dlg));

    PangoFontDescription *font_desc;
    font_desc=pango_font_description_from_string(fontname);
    gtk_widget_modify_font(window, font_desc);
    gtk_widget_modify_font (entry, font_desc);
    gtk_widget_modify_font (view, font_desc);
    gtk_widget_modify_font (view2, font_desc);
    gtk_widget_modify_font(window, font_desc);

     GKeyFile * KF;
		gchar * Value;
		GError *error = NULL;
		gsize length;
		FILE *outfile;

        char path[PATH_MAX];
        char *fname = ".BPQTermTCP.ini";
        snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"), fname);

        KF = g_key_file_new();
		g_key_file_load_from_file(KF, path, 0, NULL);

        sprintf(Font, "%s", fontname);
        g_key_file_set_string(KF, "Session 1", "Font", Font);

        Value = g_key_file_to_data(KF, &length, &error);

		outfile = fopen ( path, "w");
		fputs(Value, outfile);
		fclose(outfile);

		g_free(Value);

		g_key_file_free(KF);
}

void font_select_dlg_exit(GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy(font_select_dlg);
}


void SendTraceOptions()
{
	char Buffer[80];

	//int Len = sprintf(Buffer,"\\\\\\\\%x %x %x %x %x %x\r", PortMask, mtxparam, MCOM, MonNODES, MONColour, muionly);
    // ** makes the system use utf8 **
    int Len = sprintf(Buffer,"\\\\\\\\%x %x %x %x %x %x %x\r", PortMask, mtxparam, MCOM, MonNODES, MONColour, muionly, 1);
	send(sock, Buffer, Len, 0);

}

char Save[1000];
int SaveLen;

void WritetoOutputWindow(const char * Msg, int len)
{
	const char * ptr1 = Msg;
	char * ptr2;
	GtkTextIter iter;
	GtkTextIter enditer;
	int start, end;
	GtkTextTag *mtag;

	if (SaveLen)
	{
		// Have part line - append to it
		memcpy(&Save[SaveLen], Msg, len);
		SaveLen += len;
		ptr1 = Save;
		len = SaveLen;
		SaveLen = 0;
	}

lineloop:

	if (len <=  0)
	{
		g_timeout_add (100, ScrollTimer2, 0);
		return;
	}

	//	copy text to control a line at a time

	ptr2 = memchr(ptr1, 13, len);

	if (ptr2 == 0)	// No CR
	{
		memmove(Save, ptr1, len);
		SaveLen = len;
		return;
	}

//	*(ptr2++) = 0;

	if (ptr1[0] == 0x1b)
	{
		mtag = tag[ptr1[1] - 10];

		gtk_text_buffer_get_end_iter(text2, &iter);
		start = gtk_text_buffer_get_char_count(text2);
		gtk_text_buffer_insert(text2, &iter, ptr1 + 2, (ptr2 - ptr1) - 2);
		end = gtk_text_buffer_get_char_count(text2);

		gtk_text_buffer_get_iter_at_offset (text2, &iter, start);
		gtk_text_buffer_get_iter_at_offset (text2, &enditer, end);
		gtk_text_buffer_apply_tag (text2, mtag, &iter, &enditer);

	}
	else
	{
//		gtk_text_insert (GTK_TEXT (text), fixed_font, &text->style->black, NULL, ptr1, -1);
		gtk_text_buffer_get_end_iter(text2, &iter);
		gtk_text_buffer_insert(text2, &iter, ptr1, ptr2 - ptr1);

	}

//	gtk_text_insert (GTK_TEXT (text), fixed_font, &text->style->black, NULL, "\n", -1);
	gtk_text_buffer_get_end_iter(text2, &iter);
	gtk_text_buffer_insert(text2, &iter,  "\n", -1);
//	gtk_text_view_place_cursor_onscreen (view2);
	gtk_text_buffer_get_end_iter(text2, &iter);
	gtk_text_view_scroll_to_iter ((GtkTextView *)view2, &iter, 0.0, FALSE, 0.0, 0.0);


//	if (LogMonitor) WriteMonitorLine(ptr1, ptr2 - ptr1);

	len -= (++ptr2 - ptr1);
	ptr1 = ptr2;

	goto lineloop;

}

char MonSave[1000];
int MonSaveLen;

void WritetoMonWindow(char * Msg, int len)
{
	char * ptr1 = Msg, * ptr2;
	GtkTextIter iter;
	GtkTextIter enditer;
	int start, end;
	GtkTextTag *mtag;

	if (MonSaveLen)
	{
		// Have part line - append to it
		memcpy(&MonSave[MonSaveLen], Msg, len);
		MonSaveLen += len;
		ptr1 = MonSave;
		len = MonSaveLen;
		MonSaveLen = 0;
	}
     if (enbel)
	{
		do {
			ptr2=memchr(ptr1,7,len);

			if (ptr2)
			{
				*(ptr2)=32;
            gdk_beep();
			}
		} while (ptr2);
	}


lineloop:

	if (len <=  0)
	{
		g_timeout_add (100, ScrollTimer, 0);
		return;
	}
	//	copy text to control a line at a time

	ptr2 = memchr(ptr1, 13, len);

	if (ptr2 == 0)	// No CR
	{
		memmove(MonSave, ptr1, len);
		MonSaveLen = len;
		return;
	}

	if (ptr1[0] == 0x1b)
	{
		mtag = tagm[ptr1[1] - 10];

		gtk_text_buffer_get_end_iter(text, &iter);
		start = gtk_text_buffer_get_char_count(text);
		gtk_text_buffer_insert(text, &iter, ptr1 + 2, (ptr2 - ptr1) - 2);
		end = gtk_text_buffer_get_char_count(text);

		gtk_text_buffer_get_iter_at_offset (text, &iter, start);
		gtk_text_buffer_get_iter_at_offset (text, &enditer, end);
		gtk_text_buffer_apply_tag (text, mtag, &iter, &enditer);
	}
	else
	{
		gtk_text_buffer_get_end_iter(text, &iter);
		gtk_text_buffer_insert(text, &iter, ptr1, (ptr2 - ptr1));

	}

	gtk_text_buffer_get_end_iter(text, &iter);
	gtk_text_buffer_insert(text, &iter,  "\n", 1);

	 gtk_text_view_scroll_to_iter ((GtkTextView *)view, &iter, 0.0, FALSE, 0.0, 0.0);

//	if (LogMonitor) WriteMonitorLine(ptr1, ptr2 - ptr1);

	len -= (++ptr2 - ptr1);
	ptr1 = ptr2;

	goto lineloop;
}


int SendMsg(const char * msg, int len)
{
	send(sock, msg, len, 0);
	return 0;
}

int TCPConnect(char * Host, char * Port)
{

	int err = 0;
//	u_long param=1;
//	int bcopt=TRUE;
//	struct sockaddr_in sinx;
//	int addrlen=sizeof(sinx);
	char Title[80];

	struct addrinfo hints, *res = NULL;

	Disconnecting = FALSE;

	// get host info, make socket, and connect it

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(Host, Port, &hints, &res);

	if (!res)
	{
		dialog = gtk_message_dialog_new ((GtkWindow *)window,
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_WARNING,
                                  GTK_BUTTONS_OK,
                                  "Resolve HostName Failed");

		gtk_window_set_title (GTK_WINDOW (dialog), "TermTCP");

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);


		sprintf(Title,"TermTCP Version %s - Disconnected", VersionString);

		gtk_window_set_title (GTK_WINDOW (window), Title);

		return FALSE;			// Resolve failed

	}

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	if (connect(sock, res->ai_addr, res->ai_addrlen) == 0)
	{
		//
		//	Connected successful
		//

		Telnet_Connected(sock, 0);

		return TRUE;
	}
	else
	{
		err=errno;

		if (err == 10035)
		{
			//	Connect in Progress

			sprintf(Title,"BPQTermTCP Version %s - Connecting to %s", VersionString, Host);
			gtk_window_set_title (GTK_WINDOW (window), Title);

			EnableDisconnectMenu();
			DisableConnectMenu();

			return TRUE;
		}
		else
		{
			//	Connect failed

			closesocket(sock);
			dialog = gtk_message_dialog_new ((GtkWindow *)window,
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Connect Failed");

			gtk_window_set_title (GTK_WINDOW (dialog), "BPQTermTCP");

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return FALSE;
		}
	}

	return FALSE;
}

#define MAX_MSG_LEN 512


int ProcessReceivedData()
{
	char message[MAX_MSG_LEN + 10];
	gchar * ptr;
	char * Buffptr;
	char * FEptr = 0;
	int len = 0, MonLen;

	len = recv(sock, message, MAX_MSG_LEN, 0);

	if (len <= 0)
	{
		if (Disconnecting == FALSE)
		{
			shutdown(sock, 2);		// SD_BOTH
			Disconnecting = TRUE;
		}
		else
			if (SocketActive)
				closesocket(sock);

		sprintf(Title,"BPQTermTCP Version %s - Disconnected", VersionString);
		gtk_window_set_title (GTK_WINDOW (window), Title);
		DisableDisconnectMenu();
		EnableConnectMenu();

		WritetoOutputWindow(DisMsg, strlen(DisMsg));
		SocketActive = FALSE;
		Connected = FALSE;
		MonData = FALSE;
		return TRUE;
	}

	if (len == 0)
	{
		printf("recv - len = 0\r\n");
		if (Disconnecting == FALSE)
		{
			shutdown(sock, 2);		// SD_BOTH
			Disconnecting = TRUE;
		}
		else
			closesocket(sock);

		return TRUE;
	}

	message[len] = 0;

	// Look for MON delimiters (FF/FE)

	Buffptr = message;

	if (MonData)
	{
		// Already in MON State

		FEptr = memchr(Buffptr, 0xfe, len);

		if (!FEptr)
		{
			// no FE - so send all to monitor

			WritetoMonWindow(Buffptr, len);
			return TRUE;
		}

		MonData = FALSE;

		MonLen = FEptr - Buffptr;		// Mon Data, Excluding the FE

		WritetoMonWindow(Buffptr, MonLen);

		Buffptr = ++FEptr;				// Char following FE

		if (++MonLen < len)
		{
			len -= MonLen;
			goto MonLoop;				// See if next in MON or Data
		}

		// Nothing Left

		return TRUE;
	}

MonLoop:

	ptr = memchr(Buffptr, 0xff, len);

	if (ptr)
	{
		// Buffer contains Mon Data

		if (ptr > Buffptr)
		{
			// Some Normal Data before the FF

			int NormLen = ptr - Buffptr;				// Before the FF
			WritetoOutputWindow(Buffptr, NormLen);

			len -= NormLen;
			Buffptr = ptr;
			goto MonLoop;
		}

		MonData = TRUE;

		FEptr = memchr(Buffptr, 0xfe, len);

		if (FEptr)
		{
			MonData = FALSE;

			MonLen = FEptr + 1 - Buffptr;				// MonLen includes FF and FE
			WritetoMonWindow(Buffptr+1, MonLen - 2);

			len -= MonLen;
			Buffptr += MonLen;							// Char Following FE

			if (len <= 0)
			{
				return TRUE;
			}
			goto MonLoop;
		}
		else
		{
			// No FE, so rest of buffer is MON Data

			WritetoMonWindow(Buffptr+1, len -1);		// Exclude FF
//			dorefresh();
			return TRUE;
		}
	}

	// No FF, so must be session data

	WritetoOutputWindow(Buffptr, len);
	SlowTimer = 0;

	return TRUE;
}

int Telnet_Connected(SOCKET sock, int Error)
{
	char Msg[80];
	int Len;

	// Connect Complete

	if (Error)
	{
		dialog = gtk_message_dialog_new ((GtkWindow *)window,
               GTK_DIALOG_DESTROY_WITH_PARENT,
               GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Connect Failed");

		gtk_window_set_title (GTK_WINDOW (dialog), "TermTCP");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		closesocket(sock);
		Connecting = FALSE;
		SocketActive = FALSE;

		sprintf(Title,"TermTCP Version %s - Disconnected", VersionString);
		gtk_window_set_title (GTK_WINDOW (window), Title);
		DisableDisconnectMenu();
		EnableConnectMenu();

		return 0;

	}

//	RecvChannel = g_io_channel_unix_new((gint)sock);
//	RecvChannel = g_io_channel_win32_new_socket((gint)sock);
//	g_io_channel_set_encoding (RecvChannel, NULL, NULL);
//	g_io_channel_set_flags(RecvChannel, G_IO_FLAG_APPEND| G_IO_FLAG_NONBLOCK, NULL);
//	g_io_add_watch(RecvChannel, G_IO_IN | G_IO_HUP, GtkMsg_ShowMessage, 0);

	SocketActive = TRUE;
	Connecting = FALSE;
	Connected = TRUE;

	Len = sprintf(Msg, "%s\r%s\rBPQTermTCP\r", UserName[CurrentHost], Password[CurrentHost]);

	SendMsg(Msg, Len);

	SendTraceOptions();

	SlowTimer = 0;

	sprintf(Title,"TermTCP Version %s - Connected to %s", VersionString, Host[CurrentHost]);
	gtk_window_set_title (GTK_WINDOW (window), Title);
	DisableConnectMenu();
	EnableDisconnectMenu();

	return 0;
}

static void Disconnect(GtkWidget *w, gpointer   data)
{
	if (Disconnecting)
	{
		// Force close

			if (SocketActive)
				closesocket(sock);

			sprintf(Title,"TermTCP Version %s - Disconnected", VersionString);
			gtk_window_set_title (GTK_WINDOW (window), Title);

			DisableDisconnectMenu();
			EnableConnectMenu();


			WritetoOutputWindow(DisMsg, strlen(DisMsg));
			SocketActive = FALSE;
			Connected = FALSE;
			Disconnecting = FALSE;
			return;
	}
	shutdown(sock, 2);		// SD_BOTH
	Disconnecting = TRUE;
}

void EnableDisconnectMenu()
{
	gtk_widget_set_sensitive(dis_item, TRUE);
}
void DisableConnectMenu()
{
	gtk_widget_set_sensitive(conn_item, FALSE);
}
void EnableConnectMenu()
{
	gtk_widget_set_sensitive(conn_item, TRUE);
}
void DisableDisconnectMenu()
{
	gtk_widget_set_sensitive(dis_item, FALSE);

}

void ReadConfig()
{
	FILE *infile;
	GKeyFile * KF;
//	gchar * Value;
	gchar * Posn;
//    gchar * font;

	GError *error = NULL;
	char path[PATH_MAX];
	char *fname = ".BPQTermTCP.ini";
	snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"), fname);

	KF = g_key_file_new();
	g_key_file_load_from_file(KF, path, 0, NULL);

	Posn  = g_key_file_get_string (KF, "Session 1", "Position", &error);
	if (Posn)
		sscanf(Posn,"%d,%d,%d,%d,%d",&gx,&gy, &width, &height, &vhandle);

	printf("%s %d %d %d %d %d\n", Posn, gx, gy, width, height, vhandle);

    fontname = g_key_file_get_string (KF, "Session 1", "Font", &error);
    if (fontname)
		strcpy(Font, fontname);

    PortMask = g_key_file_get_integer(KF, "Session 1", "PortMask", &error);
	Bells = g_key_file_get_integer(KF, "Session 1", "Bells", &error);
	MCOM = g_key_file_get_integer(KF, "Session 1", "MCOM", &error);
	MONColour = g_key_file_get_integer(KF, "Session 1", "MONColour", &error);
	MonNODES= g_key_file_get_integer(KF, "Session 1", "MonNODES", &error);
	MonPorts= g_key_file_get_integer(KF, "Session 1", "MonPorts", &error);
	ChatMode= g_key_file_get_integer(KF, "Session 1", "ChatMode", &error);
	CurrentHost = g_key_file_get_integer (KF, "Session 1", "CurrentHost", &error);
	mtxparam = g_key_file_get_integer (KF, "Session 1", "MTX", &error);
//    fontname = g_key_file_intger (KF, "Session 1", "FontName", &error);
//   charset = g_key_file_intger (KF, "Session 1", "CharSet", &error);
//    codepage = g_key_file_intger (KF, "Session 1", "CodePage", &error);
//    fontsize = g_key_file_intger (KF, "Session 1", "FontSize", &error);
//    fontwidth = g_key_file_intger (KF, "Session 1", "FontWidth", &error);
    muionly = g_key_file_get_integer (KF, "Session 1", "MUIONLY", &error);

	g_key_file_free(KF);

	infile = fopen ( path, "r");

	if (infile)
	{
		char buffer[1024];
		char * ret;
		char * ptr;

		while (1)
		{
			ret = fgets(buffer, 1024, infile);

			if (ret == 0)
			{
				fclose (infile);
				return;
			}

			ptr = strchr(buffer, 10);

			if (ptr)
				*ptr = 0;

			if (memcmp(buffer, "Host", 4) == 0)
			{
				int port = atoi(&buffer[4]) - 1;
				strcpy(&Host[port][0], &buffer[6]);
				continue;
			}
			if (memcmp(buffer, "Port", 4) == 0)
			{
				int port = atoi(&buffer[4]) - 1;
				strcpy(&Port[port][0], &buffer[6]);
				continue;
			}
			if (memcmp(buffer, "User", 4) == 0)
			{
				int port = atoi(&buffer[4]) - 1;
				strcpy(&UserName[port][0], &buffer[6]);
				continue;
			}
			if (memcmp(buffer, "Pass", 4) == 0)
			{
				int port = atoi(&buffer[4]) - 1;
				strcpy(&Password[port][0], &buffer[6]);
				continue;
			}
		}
	}
//    return 0
}
