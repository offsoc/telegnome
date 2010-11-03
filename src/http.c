/* http.c */
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
  
#include <string.h>

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "http.h"
#include "main.h"
#include "prefs.h"

/*
 * get the pagenumber from the entrybox 
 */
int
get_page_entry (const gchar *page_entry)
{	
	guint page_nr;
	guint subpage_nr=0;
	gchar *page_entry_copy = g_strdup(page_entry);

	errno=0;

	/* TODO: Clean this up ;) */
	if (strlen (page_entry_copy) > 3) {  
		/* split in page and subpage */
		gchar *subpage_entry= &page_entry_copy[TG_PAGE_SIZE + 1];
		page_entry_copy[TG_PAGE_SIZE]='\0';
		subpage_nr= strtol(subpage_entry,NULL,10);
	}
   
	page_nr= strtol(page_entry_copy,NULL,10);
	if (0 != errno) {
		g_free(page_entry_copy);
		return -1;
	}
   
	currentview->page_nr=    page_nr;
	currentview->subpage_nr= subpage_nr;

	g_free(page_entry_copy);
	return 0;
}

/*
 * get the image from a remote site
 * if all's ok, return name in a GdkPixbuf
 */
gint
get_the_image (GdkPixbuf **pixbuf)
{
    gchar http_query[100];
    gint retval=0;
    GFile *http_file;
    GFileInputStream *http_input;
    GdkPixbufLoader *loader = NULL;
    guchar buf[4096];
    gssize bytes_read;
    GError *err = NULL;
    
    if ( -1 == get_http_query(http_query, currentview->page_nr, currentview->subpage_nr))	
	return TG_ERR_HTTPQUERY;

    /* get the image from remote server */
    http_file = g_file_new_for_uri(http_query);
    http_input = g_file_read(http_file, NULL, NULL);
    if (!http_input)
	return TG_ERR_VFS;

    loader = gdk_pixbuf_loader_new();

    for (;;) {
	bytes_read = g_input_stream_read(G_INPUT_STREAM(http_input), buf, 4096,
					 NULL, NULL);
	if (bytes_read == 0)
	    break;
	err = NULL;
	if (!gdk_pixbuf_loader_write(loader, buf, (gsize)bytes_read, &err)) {
	    retval = TG_ERR_PIXBUF;
	    goto out;
	}
    }

    *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (!*pixbuf) {
	retval = TG_ERR_PIXBUF;
	goto out;
    }
    g_object_ref(*pixbuf);

out:
    if (loader) {
	if (!gdk_pixbuf_loader_close(loader, &err) && !retval)
	    retval = TG_ERR_PIXBUF;
    }
    if (http_input)
	g_input_stream_close(G_INPUT_STREAM(http_input), NULL, NULL);

    return retval;
}

int
get_http_query (gchar* buffer, gint page_nr, gint subpage_nr)
{
    if ( subpage_nr>0 ) {    /* do we have a subpage? */
	sprintf (  buffer, 
		   currentview->channel->subpage_url->str, 
		   page_nr, 
		   subpage_nr);
    } else {
	sprintf (  buffer, 
		   currentview->channel->page_url->str,
		   page_nr);
    }
    return 0;
}
