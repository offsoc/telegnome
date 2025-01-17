/*
 * view.c
 * The viewer component of TeleGNOME
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

#include "view.h"
#include "prefs.h"
#include "main.h"
#include "http.h"
#include "pixpack.h"

TeleView *
tele_view_new()
{
    TeleView *v;
    GtkWidget *l, *sw;
    
    v = g_malloc(sizeof(TeleView));

    v->box = gtk_vbox_new(TRUE, 0);
    
    v->pixpack = pixpack_new();
    pixpack_set_autosize( PIXPACK(v->pixpack), TRUE );

    gtk_box_pack_start_defaults(GTK_BOX(v->box), v->pixpack);
    
    v->zoom_factor = 1.0;

    v->page_nr = -1;
    v->subpage_nr = -1;

    return v;
}

void 
tele_view_set_error_handler(TeleView *view, void (*e)(const char *))
{
    view->error_handler = e;
}

void 
tele_view_error(TeleView *view, const char *c)
{
    g_print("Error: %s\n", c);
}

gint
tele_view_update_pixmap(TeleView *view, gchar *filename)
{
    gint width, height;
    if (g_file_exists) {
	pixpack_load_image_file( PIXPACK(view->pixpack), filename);
    } else {
	/* no existing file, resize to a default and print a warning */
	g_warning("File doesnt exist: %s\n", filename);
	gtk_widget_set_usize(GTK_WIDGET(view->pixpack), 200, 150);
    }

    return 0;
}

gint
tele_view_update_page(TeleView *view, int *major_nr, int *minor_nr)
{
 	gint retval;
	gchar *picfile;

	/* save these the restore the, if necessary */
	gint old_page= *major_nr;
	gint old_subpage= *major_nr;

	/* make http-request, returns the file of the saved thing */
	retval= get_the_image(&picfile);

	if (TG_OK == retval) {
		tele_view_update_pixmap(view, picfile);
		return 0;
	} else {
		switch(retval) {
		case TG_ERR_TOOSMALL:	/* we got an error from the webpage */
		    /* maybe we forgot the subpage nr, or used it when we shouldn't */
		    *minor_nr= (0 == *minor_nr)?1:0;
		    if (TG_OK != get_the_image(&picfile)) { 
			if (*minor_nr!=1) {
				/* maybe we've run out of subpages, go to next main page */
			    *minor_nr=0;
			    (*major_nr)++;
			    update_entry(*major_nr, *minor_nr);
			    get_the_page(FALSE); /* dont redraw */ 
			} else {
			    (*(view->error_handler))(_("Web server error: Wrong page number?"));
			    *major_nr= old_page;  /* restore */
			    *minor_nr= old_subpage;
			    update_entry(*major_nr, *minor_nr);
			    tele_view_update_pixmap(view, gnome_pixmap_file(TG_NOTFOUND_PIXMAP));
			    return -1;
			}
		    } else {
			tele_view_update_pixmap(view, picfile);
			return 0;
		    }		
		case TG_ERR_GHTTP:
		    tele_view_error(view, _("Error making HTTP connection"));
		    return -1;
		case TG_ERR_NOCONNECTION:
		    tele_view_error(view, _("No connection"));
		    return -1;
		case TG_ERR_FILE:
		    tele_view_error(view, _("Error using temporary file"));
		    return -1;
		case TG_ERR_CHMOD:
		    tele_view_error(view, _("Error setting permissions on temporary file"));
		    return -1;
		case TG_ERR_HTTPQUERY:
		    tele_view_error(view, _("Internal error in HTTP query code"));
		    return -1;
		default: 
		    g_assert_not_reached();
		    return -1;
		}
	}
	return 0;
}

GtkWidget *
tele_view_get_widget(TeleView *view)
{
    return view->box;
}

void 
tele_view_free(TeleView *view)
{
    /* clean up */
    gtk_widget_destroy(GTK_WIDGET(view->box));
    g_free(view);
}

