/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2014/2015
 *
 * callbacks.c
 *
 * Functions that handle main application logic for UDP communication, controlling query forwarding
 *
 * Updated on October 20, 16:00
 * @author  Luis Bernardo
\*****************************************************************************/

#include <gtk/gtk.h>
#include <arpa/inet.h>
#include <assert.h>
#include <time.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include "sock.h"
#include "gui.h"
#include "callbacks.h"
#include "callbacks_socket.h"

#ifdef DEBUG
#define debugstr(x)     g_print(x)
#else
#define debugstr(x)
#endif

/**********************\
|*  Global variables  *|
 \**********************/

gboolean active = FALSE; 	// TRUE if server is active


/*********************\
|*  Local variables  *|
 \*********************/

// Temporary buffer used for writing, logging, etc.
static char tmp_buf[8000];

// Local functions
gboolean callback_query_timeout(gpointer data);

GList *qList = NULL;
guint query_timer;



/**************************************************\
|* Functions that handle TCP socket communication *|
\**************************************************/

/* Change the IO event monitored in a socket to IO_READ, IO_WRITE, or (IO_READ|IO_WRITE) */
gboolean set_socket_callback_condition_in_mainloop(int sock, void *ptr, guint *chan_id, GIOChannel *chan,
		guint event, gboolean(*callback)(GIOChannel *, GIOCondition, gpointer)) {
	assert(chan_id != NULL);
	assert(chan != NULL);
	assert(callback != NULL);
	if (*chan_id > 0) {
		suspend_socket_from_mainloop(sock, *chan_id);
		*chan_id= 0;
	}
	if (event != 0) {
		return restore_socket_in_mainloop(sock, ptr, chan_id, chan, event, callback);
	} else {
		return TRUE;
	}
}


/* Example callback function that handles reading/writing events from a TCP socket
 * It returns TRUE to keep the callback active, and FALSE to disable the callback */
gboolean
callback_TCP_socket (GIOChannel *source, GIOCondition condition, gpointer data)
{
	static char write_buf[1024];
//	char buf[MSG_BUFFER_SIZE];

//	int s= g_io_channel_unix_get_fd(source); // Get the socket file descriptor
//	int n;

	/*
	Query *pt= (Query *)data;	// Recover the pointer set when the callback was defined
	...
	*/

	if (condition & G_IO_IN)
	{
		// Data available for reading at socket s
		printf("G_IO_IN\n");

		// Use the following expression to read bulk data from the socket:
		// 		n= read(s, buf, sizeof(buf));
		// For a header field (int seq), you should use:
		// 		n= read(s, &seq, sizeof(seq));
		// Do not forget to test the value returned (n):
		//	- n==0  -  EOF (connection broke)
		//	- n<0   -  reading error in socket (test (errno==EWOULDBLOCK) if it is in non-blocking mode
		//
		// During a write operation with
		//		n= write(s, buf, m);
		//  it may return n != m when it is in the non-blocking mode!
		//  If n==-1 and (errno==EWOULDBLOCK), or if (n>0) means that the TCP's output buffer if full.
		//      You should enable the G_IO_OUT event in the main loop using the function
		//	set_socket_callback_condition_in_mainloop, and wait before continuing sending data.
		//  Store the pending data not sent in a buffer in the struct Query, so you can resend it again latter
		//  when the event G_IO_OUT is received
		//
		// A socket sock can be set to non blocking mode using:
		//		fcntl(sock,F_SETFL,O_NONBLOCK);

		return TRUE;	// If there is more data coming, otherwise return FALSE;

	} else if (condition & G_IO_OUT) {
		// Space available for reading at socket s
		printf("G_IO_OUT\n");

		// This event should only be used when a write/send operation previously returned -1  with (errno==EWOULDBLOCK),
		//     or a number of bytes below the number of bytes written. This means that the TCP's output buffer is full!
		//
		//	When the remaining bytes are written, do not forget to remove the G_IO_OUT event from the mainloop,
		//		using the function set_socket_callback_condition_in_mainloop

		return TRUE;  // If there is more data coming, otherwise return FALSE;
	} else if ((condition & G_IO_NVAL) || (condition & G_IO_ERR)) {
		printf("G_IO_NVAL or G_IO_ERR\n");
		sprintf(write_buf, "G_IO_NVAL or G_IO_ERR - socket error %d\n", condition);
		Log(write_buf);
		// The query is broke - remove it
		return FALSE;	// Removes socket's callback from main cycle
	}
	else
	{
		assert (0);		// Must never reach this line - aborts application with a core dump
		return FALSE;	// Removes socket's callback from main cycle
	}
}


// Handle the reception of a new connection on a server socket
// Return TRUE if it should accept more connections; FALSE otherwise
gboolean handle_new_connection(int sock, void *ptr, struct sockaddr_in6 *cli_addr) {
	// You should put enough information in ptr to implement this function
	//  My recommendation is that you pass a pointer to the Query struct associated to the Query
	Query *pt= (Query *)ptr;

	// Update the information about the client socket that connected
	//		GUI_update_cli_details_Proxy(port_SS / *put here the server socket port number* /,
	//			g_strdup(addr_ipv6(&cli_addr->sin6_addr)), ntohs(cli_addr->sin6_port));

	// You should create a new TCP socket and connect it to the remote server, with the IP:port received in the HIT packet
	// Then, you start the two data callbacks for socket sock (connected to the client) and for the new socket
	//    (connected to the server).
	//		   for the client side you may use the function init_client_socket_tcp6 (complete it)
	//         for the server side, you just need to setup the callback!
	// Use the callback_TCP_socket function above as a model for the two callbacks you need to implement and adapt it
	//	   to run as client or as server; read carefully the comments within the function.

	// Update the information about the remote server
	// 		GUI_update_serv_details_Proxy(port_SS / *put here the server socket port number* /,
	//			g_strdup(server_ipv6_str), server_port);

	// Don't forget to configure your socket to maximize throughput
	//	e.g. SO_SNDBUF, non-blocking, etc.
	//
	return FALSE;	// Return FALSE to close the callback, avoiding other connections to the server socket
}




/*******************************************************\
|* Functions to control the state of the application   *|
 \*******************************************************/

// Extracts the directory name from a full path name
static const char *get_trunc_filename(const char *FileName) {
	char *pt = strrchr(FileName, (int) '/');
	if (pt != NULL)
		return pt + 1;
	else
		return FileName;
}

// Callback that handles query timeouts
gboolean callback_query_timeout(gpointer data) {
	Query *q = (Query *) data;

	g_print("Callback query_timeout\n");

	if(q->state==1){
		printf("Sending query...\n");
		send_multicast(q->buf, q->buflen, !q->is_ipv6);
		q->state = 2;
		query_timer = g_timeout_add(10000,callback_query_timeout, q);

	} else {
		printf("Query timed out!!");
		remove_from_qlist(q->name,q->seq,q->is_ipv6);
		GUI_del_Query(q->name,q->seq,q->is_ipv6);
	}
	// The timer when off!
	// Put here what you should do about it

	// It should depend on the query/connection state

	// If you are running a jitter timer before transmitting the Query, you should send the Query
	// 	The jitter value can be
	//			long int jitter_time = (long) floor(1.0 * random() / RAND_MAX * QUERY_JITTER);

	// If it is waiting for a Hit, cancel the pending Query

	// If it is waiting for a connection, you should cancel the server socket and the pending Query

	return FALSE;	// Stops the timer
	// return TRUE;	  // Keeps the timer running
}

Query* put_in_qlist(const char* fname, int seq, gboolean is_ipv6, struct in6_addr *ipv6, struct in_addr *ipv4, u_short port, char* buf, int buflen){

	struct Query *pt = (struct Query*)malloc(sizeof(struct Query));

	memcpy(&pt->ipv4, ipv4, sizeof(struct in_addr));
	memcpy(&pt->ipv6, ipv6, sizeof(struct in6_addr));
	memcpy(&pt->buf, buf, buflen);
	pt->buflen = buflen;
	pt->is_ipv6=is_ipv6;

	strncpy ( pt->name, fname, sizeof(pt->name));
	pt->port1 = port;
	pt->seq = seq;
	pt->state = 1;
	qList = g_list_append(qList,pt);
	return pt;
}

Query* get_from_qlist(const char* fname, int seq, gboolean is_ipv6){
	Query* aux;

	if(qList != NULL){
		qList = g_list_first(qList);
		while(qList!= NULL){
			aux = (Query*) qList->data;
			if(!strcmp(fname, aux->name) && seq == aux->seq && is_ipv6 == aux->is_ipv6)
				return aux;
			qList = g_list_next(qList);
		}
		return NULL;
	} else return NULL;
}

void remove_from_qlist(const char* fname, int seq, gboolean is_ipv6){
	Query* aux;
	aux = get_from_qlist(fname, seq, is_ipv6);
	qList = g_list_remove(qList, aux);
	free(aux);
}


// Handle the reception of a Query packet
void handle_Query(char *buf, int buflen, gboolean is_ipv6,
		struct in6_addr *ipv6, struct in_addr *ipv4, u_short port) {
	int seq;
	gboolean is_located = FALSE;
	const char *fname;

	assert((buf != NULL) && ((is_ipv6&&(ipv6!=NULL)) || (!is_ipv6&&(ipv4!=NULL))));
	if (!read_query_message(buf, buflen, &seq, &fname)) {
		Log("Invalid Query packet\n");
		return;
	}

	assert(fname != NULL);
	if (strcmp(fname, get_trunc_filename(fname))) {
		Log("ERROR: The Query must not include the pathname - use 'get_trunc_filename'\n");
		return;
	}

	char tmp_ip[100];
	if (is_ipv6)
		strcpy(tmp_ip, addr_ipv6(ipv6));
	else
		strcpy(tmp_ip, addr_ipv4(ipv4));
	if (is_local_ip(tmp_ip) && (port == portUDPq)) {
		// Ignore local loopback
		return;
	}

	sprintf(tmp_buf, "Received Query '%s'(%d) from [%s]:%hu\n", fname, seq,
			(is_ipv6 ? addr_ipv6(ipv6) : addr_ipv4(ipv4)), port);
	Log(tmp_buf);


	//Log("Handle Query not implemented yet\n");
	// Start here!
	const char* str_ip;
	unsigned int GUIport;
	const char* hits;

	// Check if it is a new query (equal name+seq+is_ipv6) - ignore if it is an old one
	// Check if the query has appeared in the !is_ipv6 domain - if it has, ignore it because someone else send it before.
	is_located = GUI_get_Query_details(fname, seq, is_ipv6,&str_ip, &GUIport, &hits);
	if(!is_located){
		if(GUI_get_Query_details(fname, seq, !is_ipv6, &str_ip, &GUIport, &hits)){
			return;
		}
	} else return;
	// Add the query to the graphical Query list
	GUI_add_Query(fname, seq, is_ipv6, strdup(tmp_ip), port);
	// If it is new, create a new Query struct and store it in your list
	// Store query information in list and start jitter Timer
	Query* query = put_in_qlist(fname, seq, is_ipv6, ipv6, ipv4, port, buf, buflen);

	// If you think that this is just too much for you, just use the graphical list -- No prob

	// Start by forwarding here the Query message to the other domain (i.e. !is_ipv6) using:
	long int jitter_time = (long) floor(1.0 * random() / RAND_MAX * QUERY_JITTER);

	query_timer = g_timeout_add(jitter_time, callback_query_timeout, query);
	//
	// At the end, if you have time, start here a jitter timer, which will send the Query later!
	//	This helps when there are more than one gateway connecting two multicast groups!


}


// Handle the reception of an Hit packet
void handle_Hit(char *buf, int buflen, struct in6_addr *ip, u_short port,
		gboolean is_ipv6) {

	int seq;
	const char *fname;
	unsigned long long flen;
	unsigned long long fhash;
	unsigned short sTCP_port;

	if (!read_hit_message(buf, buflen, &seq, &fname, &flen, &fhash,
			&sTCP_port)) {
		Log("Invalid Hit packet\n");
		return;
	}

	sprintf(tmp_buf,
			"Received Hit '%s'(%d)%s (IP= %s; port= %hu; Len=%llu; Hash=%llu)\n",
			fname, seq, (is_ipv6?"IPv6":"IPv4"), addr_ipv6(ip), sTCP_port, flen, fhash);
	Log(tmp_buf);

	// If you have implemented the Query list, test here if this HIT matches one of the Query contents


	// Add HIT to the graphical list
	sprintf(tmp_buf, "%s-%hd", addr_ipv6(ip), sTCP_port);
	GUI_add_hit_to_Query(fname, seq, !is_ipv6, tmp_buf);

	Log("Handle_Hit not implemented yet -- but being implemented\n");

	if (!is_ipv6) {
		// HIT received from an IPv4 server
		Query* q;
		// Send the HIT message to the client
		// You may get the client information from your Query list


		q = get_from_qlist(fname, seq, !is_ipv6);

		// If you did not do it, you may also get the client's information from the graphical table using
		//GUI_get_Query_details(fname, seq, !is_ipv6, const char **str_ip, unsigned int *port, const char **hits);
		//	   str_ip has the IP address and port has the port number.
		// Send the HIT packet to the client
		//    you may use the function send_M6reply ...
		send_M6reply(&q->ipv6,q->port1, buf, buflen);
		// In order to avoid not seeing the Query in the graphical table, I recommend that you let the timer clear it.
		// Wait for timeout to clear the GUI entry
		// Otherwise, you can clear it here using:
		//		GUI_del_Query(fname, (*ppt)->seq, (*ppt)->is_ipv6);
		return;
	}

	// HIT received from an IPv6 server
	// In this case you need to create a new proxy server
	// You should use fields in the Query struct to store all the information about the proxy
	Query* q;
	q = get_from_qlist(fname, seq, !is_ipv6);

	// Create a new server socket using:
	init_server_socket_tcp6 (&q->chanTCP,&q->chanTCP_id,q);
	// Store all the socket information in the Query structure

	// Write the proxy information in the proxies list
	//		GUI_add_Proxy(fname, seq, proxy_server_socket_port);

	// Prepare a new HIT message with the proxy information and send it to the client.
	// Use
	//		send_message4(&client_ipv4_address, client_port, HIT_buffer, HIT_buflen)

	// Restart the timer, to wait for QUERY_TIMEOUT seconds for a connection

}


// Callback button 'Stop': stops the selected TCP transmission and associated proxy
void on_buttonStop_clicked(GtkButton *button, gpointer user_data) {
	GtkTreeIter	iter;
	const char *fname;
	int seq;
	u_int Tport;


	if (GUI_get_selected_Proxy(&fname, &seq, &Tport, &iter)) {
#ifdef DEBUG
		g_print ("Proxy with port %d will be stopped\n", Tport);
#endif
	} else {
		Log ("No proxy selected\n");
		return;
	}
	if (Tport <= 0) {
		Log("Invalid TCP port in the selected line\n");
		return;
	}

	gtk_list_store_remove(main_window->listProxies, &iter);

	// Stop proxy
	// ...
	Log("on_buttonStop_clicked not implemented yet\n");

}


// Closes everything
void close_all(void) {
	// Stop all proxies active
	Log("close_all does not stop the ongoing Queries and Proxies yet\n");
	// Close all sockets
	close_sockUDP();
}


// Button that starts and stops the application
void on_togglebutton1_toggled(GtkToggleButton *togglebutton, gpointer user_data) {

	if (gtk_toggle_button_get_active(togglebutton)) {

		// *** Starts the server ***
		const gchar *addr4_str, *addr6_str;
		int n4, n6;

		n4 = get_PortIPv4Multicast();
		n6 = get_PortIPv6Multicast();
		if ((n4 < 0) || (n6 < 0)) {
			Log("Invalid multicast port number\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			return;
		}
		port_MCast4 = (unsigned short) n4;
		port_MCast6 = (unsigned short) n6;

		addr6_str = get_IPv6Multicast(NULL);
		addr4_str = get_IPv4Multicast(NULL);
		if (!addr6_str && !addr4_str) {
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			return;
		}
		if (!init_sockets(port_MCast4, addr4_str, port_MCast6, addr6_str)) {
			Log("Failed configuration of server\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			return;
		}
		//set_PortTCP(port_TCP);
		set_PID(getpid());
		//
		block_entrys(TRUE);
		active = TRUE;
		Log("gateway active\n");

	} else {

		// *** Stops the server ***
		close_all();
		block_entrys(FALSE);
		set_PID(0);
		active = FALSE;
		Log("gateway stopped\n");
	}

}


// Callback function that handles the end of the closing of the main window
gboolean on_window1_delete_event(GtkWidget * widget, GdkEvent * event,
		gpointer user_data) {
	gtk_main_quit();	// Close Gtk main cycle
	return FALSE;		// Must always return FALSE; otherwise the window is not closed.
}

