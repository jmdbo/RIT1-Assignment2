/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2014/2015
 *
 * main.c
 *
 * Main function
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
#include "gui.h"
#include "sock.h"
#include "callbacks.h"

/* Public variables */
WindowElements *main_window;	// Pointer to all elements of main window


int main (int argc, char *argv[]) {
    /* allocate the memory needed by our TutorialTextEditor struct */
    main_window = g_slice_new (WindowElements);


    /* initialize GTK+ libraries */
    gtk_init (&argc, &argv);

    if (init_app (main_window) == FALSE) return 1; /* error loading UI */
	gtk_widget_show (main_window->window);


#ifdef __i386__
	Log("You are in a 32 bit OS\n");
#endif
#ifdef __x86_64__
	Log("You are in a 64 bit OS\n");
#endif

	// Get local IP
	set_local_IP();
	if (valid_local_ipv4)
		set_LocalIPv4(addr_ipv4(&local_ipv4));
	if (valid_local_ipv6)
		set_LocalIPv6(addr_ipv6(&local_ipv6));

	// Infinite loop handled by GTK+3.0
	gtk_main ();

    /* free memory we allocated for TutorialTextEditor struct */
    g_slice_free (WindowElements, main_window);

	return 0;
}

