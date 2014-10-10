/*
 * gui.h
 */

/*
**    Copyright (C) 1999, 2000,
**    Dirk-Jan C. Binnema <djcb@dds.nl>,
**    Arjan Scherpenisse <acscherp@wins.uva.nl>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifndef _GUI_H_
#define _GUI_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include "view.h"

#define TG_MAX_CHANNELS 100

GtkWidget *tg_gui_new ();

int tg_gui_update_entry (gint page_nr, gint subpage_nr);
void tg_gui_get_the_page (gboolean redraw);


/* event handler callbacks */
void tg_gui_cb_quit (GtkWidget* widget, gpointer data);
void tg_gui_cb_about (GtkWidget* widget, gpointer data);
void tg_gui_cb_preferences (GtkWidget* widget, gpointer data);
void tg_gui_cb_next_page (GtkWidget* widget, gpointer data);
void tg_gui_cb_prev_page (GtkWidget* widget, gpointer data);
void tg_gui_cb_home (GtkWidget* widget, gpointer data);
void tg_gui_cb_goto_page (GtkWidget* widget, gpointer data);
void tg_gui_cb_zoom (GtkWidget *widget, gpointer data);
gint tg_cb_keypress (GtkWidget *widget, GdkEventKey *event);

/* some widgets we need a runtime ref to */
typedef struct _TgGui {
    GtkWidget *app;
    
    GtkWidget *statusbar;
    GtkWidget *entry;
    GtkWidget *pixmap;
    GtkWidget *eventbox; 
    
    /* for session management */
    GnomeClient *client;
    
    GtkProgressBar *progress;
    
    GtkWidget *zoomlabel;
    gint zoom_factor;

    GtkWidget *zoombutton;
    GtkWidget *pagebutton;

    GtkWidget *channel_menu;

    /* for timer-input */
    gint logo_timer;
    gint kb_timer;
    gint kb_status;

    gint page_timer;
    gint page_msecs;
    gint page_status; /* auto-paging enabled or disabled */
    gint page_progress;

    GSList *channels;
    gint default_server;

    /* FIXME: Multiple views */
} TgGui;

/* DnD target types */
enum {
	TARGET_GIF=0,
	TARGET_TXT,
	TARGET_FILE,
	TARGET_ROOTWIN
};

enum {
    INPUT_NEW=0,
    INPUT_CONTINUED
};

#endif /* _GUI_H_ */
