/*
 * gui.c 
 */

/*
**    Copyright (C) 1999, 2000,
**    Dirk-Jan C. Binnema <djcb@dds.nl>,
**    Arjan Scherpenisse <acscherp@wins.uva.nl>
**    Copyright (C) 2016 Colin Watson <cjwatson@debian.org>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gui.h"
#include "main.h"
#include "prefs.h"
#include "channel.h"

struct _TgGui {
    GObject parent_instance;

    GtkWidget *window;
    GtkAccelGroup *accel_group;
    GtkWidget *grid;
    GtkWidget *progress_bar;
    GtkWidget *status_bar;

    GSettings *settings;

    GtkWidget *entry;
    GtkWidget *pixmap;

    GtkWidget *zoombutton;

    GMenu *channel_menu;

    /* for timer-input */
    gint logo_timer;
    gint kb_timer;
    gint kb_status;

    gint page_timer;
    gint page_msecs;
    gboolean page_status; /* auto-paging enabled or disabled */
    gint page_progress;

    /* FIXME: Multiple views */

    gchar **channel_children;
    GSList *channels;
    gchar *current_channel;
};

enum {
    PROP_0,

    PROP_CHANNEL_CHILDREN,
    PROP_CURRENT_CHANNEL,
    PROP_ZOOM_FACTOR,
    PROP_PAGING_ENABLED,
    PROP_PAGING_INTERVAL,
    PROP_CURRENT_PAGE_NUMBER,
    PROP_CURRENT_SUBPAGE_NUMBER
};

G_DEFINE_TYPE (TgGui, tg_gui, G_TYPE_OBJECT)

static TgGui *gui;

static void
tg_gui_update_title_bar(void)
{
    char buf[100];
    /* update the title bar */
    if (currentview != NULL && currentview->channel != NULL) {
	gchar *name, *desc;
	g_object_get(
	    currentview->channel, "name", &name, "description", &desc, NULL);
	if (name != NULL && desc != NULL) {
	    sprintf(buf, _("TeleGNOME: %s (%s)"), name, desc);
	    gtk_window_set_title(GTK_WINDOW(gui->window), buf);
	}
	g_free(desc);
	g_free(name);
    }
}

/* removes the logo from the screen and goes to a page */
static gint 
tg_gui_logo_timer(gpointer g) 
{
    if (gui->logo_timer != -1)
	g_source_remove(gui->logo_timer);
    gui->logo_timer = -1;
    tg_gui_get_the_page(FALSE);
    return 0;
}

static TgChannel *
tg_gui_channel_find_by_uuid(const gchar *uuid)
{
    GSList *cur;

    for (cur = gui->channels; cur; cur = g_slist_next(cur)) {
	gchar *cur_uuid;
	g_object_get(G_OBJECT(cur->data), "uuid", &cur_uuid, NULL);
	if (cur_uuid && g_str_equal(uuid, cur_uuid)) {
	    g_free(cur_uuid);
	    return TG_CHANNEL(g_object_ref(G_OBJECT(cur->data)));
	}
	g_free(cur_uuid);
    }
    return NULL;
}

/* changes the channel */
static void 
tg_gui_channel_select(TgChannel *channel)
{
    g_assert(channel != NULL);

    if (currentview->channel)
	g_object_unref(G_OBJECT(currentview->channel));
    currentview->channel = g_object_ref(G_OBJECT(channel));

    tg_gui_update_title_bar();

#if 0
    {
	gchar *name, *desc;
	g_object_get(channel, "name", &name, "desc", &desc, NULL);
	g_print("Channel Selected: %s (%s)\n", name, desc);
	g_free(desc);
	g_free(name);
    }
#endif
}

void
tg_gui_change_state_set_channel(GSimpleAction *action, GVariant *value,
				gpointer data)
{
    const gchar *uuid;

    uuid = g_variant_get_string(value, NULL);
    g_object_set(gui,
		 "current-channel", uuid,
		 "current-page-number", 100,
		 "current-subpage-number", 0,
		 NULL);
    tg_gui_get_the_page(FALSE);
}

/*************************
 * create the channel menu
 */
static void
tg_gui_populate_channel_menu(void)
{
    int i;
    TgChannel *channel;

    g_assert(gui->channels != NULL);

    g_menu_remove_all(gui->channel_menu);

    for (i=0; i<g_slist_length(gui->channels); i++) {
	gchar *name, *uuid, *action;

	channel = TG_CHANNEL(g_slist_nth_data(gui->channels, i));
	g_object_get(channel, "name", &name, "uuid", &uuid, NULL);
	action = g_strdup_printf("app.set-channel::%s", uuid);
	g_menu_append(gui->channel_menu, name, action);
	g_free(action);
	g_free(uuid);
	g_free(name);
    }
}

/*************************
 * Loads all the channels from the config and puts them in the
 * gui->channels GSList
 */
static void
tg_gui_reload_channels(void)
{
    gchar **childp;
    TgChannel *channel;
    gchar *current_uuid = NULL;

    if (gui->channels != NULL) {
	g_slist_free_full(gui->channels, g_object_unref);
	gui->channels = NULL;
    }

    current_uuid = g_strdup(gui->current_channel);

    for (childp = gui->channel_children; childp && *childp; ++childp) {
	channel = tg_channel_new(*childp, NULL);
	if (channel)
	    gui->channels = g_slist_append(gui->channels, (gpointer)channel);
    }
    if (!gui->channels) {
	/* nothing set up yet, fill in some defaults */
	/* TODO: This is terrible; move into a separate file. */
	gchar **children = g_new0(gchar *, 6);
	int i = 0;

	channel = tg_channel_new(
	    NULL,
	    "name", "Teletext ČT, Czech Republic",
	    "description", "Czech teletext",
	    "page-url", "http://www.ceskatelevize.cz/services/teletext/picture.php?channel=CT1&page=%03d",
	    "country", "cz",
	    NULL);
	gui->channels = g_slist_append(gui->channels, (gpointer)channel);
	g_object_get(channel, "uuid", &children[i++], NULL);
	channel = tg_channel_new(
	    NULL,
	    "name", "YLE Teksti-TV, Finland",
	    "description", "Finnish teletext (YLE)",
	    "page-url", "http://www.yle.fi/tekstitv/images/P%03d_01.gif",
	    "subpage-url", "http://www.yle.fi/tekstitv/images/P%03d_%02d.gif",
	    "country", "fi",
	    NULL);
	gui->channels = g_slist_append(gui->channels, (gpointer)channel);
	g_object_get(channel, "uuid", &children[i++], NULL);
	channel = tg_channel_new(
	    NULL,
	    "name", "MTV3 Tekstikanava, Finland",
	    "description", "Finnish teletext (MTV3)",
	    "page-url", "http://www.mtvtekstikanava.fi/new2008/images/%03d-01.gif",
	    "subpage-url", "http://www.mtvtekstikanava.fi/new2008/images/%03d-%02d.gif",
	    "country", "fi",
	    NULL);
	gui->channels = g_slist_append(gui->channels, (gpointer)channel);
	g_object_get(channel, "uuid", &children[i++], NULL);
	channel = tg_channel_new(
	    NULL,
	    "name", "MTV1, Hungary",
	    "description", "Hungarian teletext",
	    "page-url", "http://www.teletext.hu/mtv1/images/%03d-01.gif",
	    "subpage-url", "http://www.teletext.hu/mtv1/images/%03d-%02d.gif",
	    "country", "hu",
	    NULL);
	gui->channels = g_slist_append(gui->channels, (gpointer)channel);
	g_object_get(channel, "uuid", &children[i++], NULL);
	channel = tg_channel_new(
	    NULL,
	    "name", "Ceefax, United Kingdom",
	    "description", "UK teletext (BBC)",
	    "page-url", "http://www.ceefax.tv/cgi-bin/gfx.cgi?page=%03d_0&font=big&channel=bbc1",
	    "subpage-url", "http://www.ceefax.tv/cgi-bin/gfx.cgi?page=%03d_%d&font=big&channel=bbc1",
	    "country", "gb",
	    NULL);
	gui->channels = g_slist_append(gui->channels, (gpointer)channel);
	g_object_get(channel, "uuid", &children[i++], NULL);
	g_settings_set_strv(gui->settings, "channel-children",
			    (const gchar **) children);
	g_strfreev(children);
    }

    if (current_uuid)
	g_object_set(gui, "current-channel", current_uuid, NULL);
    if (!currentview->channel) {
	gchar *first_uuid;
	g_object_get(
	    TG_CHANNEL(gui->channels->data), "uuid", &first_uuid, NULL);
	g_object_set(
	    gui,
	    "current-channel", first_uuid,
	    "current-page-number", 100,
	    "current-subpage-number", 0,
	    NULL);
	g_free(first_uuid);
	if (current_uuid)
	    tg_gui_get_the_page(FALSE);
    }
    g_free(current_uuid);
}

static void
tg_gui_refresh_channel_menu(void)
{
    /* load the channels from disk */
    tg_gui_reload_channels();

    /* re-populate the menu */
    tg_gui_populate_channel_menu();
}

/*******************************
 * print a string in the statusbar
 */
static void
tg_gui_print_in_statusbar(const char *buf)  /*FIXME: buffersize*/
{
    guint context_id;

    context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(gui->status_bar),
					      "errors");
    gtk_statusbar_remove_all(GTK_STATUSBAR(gui->status_bar), context_id);
    if (buf)
	gtk_statusbar_push(GTK_STATUSBAR(gui->status_bar), context_id, buf);
}

/******************************* 
 * create a new entry 
 */
static GtkWidget * 
tg_gui_new_entry (void)
{
    GtkWidget *entry = NULL;
    gint width;

    entry = gtk_entry_new();
    width = TG_PAGE_SIZE + 1 + TG_SUBPAGE_SIZE;
    gtk_entry_set_max_length(GTK_ENTRY(entry), width);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), width);

    /*gtk_entry_set_text(GTK_ENTRY(entry), startpage);*/
	  
    g_signal_connect(G_OBJECT(entry), "activate", 
		     G_CALLBACK(tg_gui_cb_goto_page), NULL);

    /* save entry for later ref */
    gui->entry = entry;

    return entry;
}


static gint
tg_gui_pager_timer(gpointer g)
{
    gui->page_progress += gui->page_msecs/100;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gui->progress_bar), gui->page_progress / (gdouble)gui->page_msecs);

    if (gui->page_progress >= gui->page_msecs) {
	gui->page_progress = 0;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gui->progress_bar), 0.0);
	tg_gui_cb_next_page(NULL, NULL);
    }
    return 1;
}

static void 
tg_gui_cb_toggle_paging(GtkWidget *w, gpointer data) 
{
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gui->progress_bar), 0.0);
    if (gui->page_status) {
	if (gui->page_timer != -1)
	    g_source_remove(gui->page_timer);
	gui->page_timer = -1;
	gui->page_status = FALSE;
	gui->page_progress = 0;
    } else {
	gui->page_progress = 0;
	gui->page_status = TRUE;
	gui->page_timer = g_timeout_add(gui->page_msecs/100, tg_gui_pager_timer, NULL);
    }
}

/*******************************
 * create a new toolbar 
 */
static GtkWidget *
tg_gui_new_toolbar (void)
{
    GtkWidget *toolbar, *entry, *grid, *w;
    GtkToolItem *toolitem;

    toolbar = gtk_toolbar_new();

    grid = gtk_grid_new();
    gtk_orientable_set_orientation(GTK_ORIENTABLE(grid),
				   GTK_ORIENTATION_HORIZONTAL);
    
    w = gtk_label_new(_("Page:"));
    g_object_set(G_OBJECT(w), "margin", 5, NULL);
    gtk_container_add(GTK_CONTAINER(grid), w);

    /* add the entry */
    entry = tg_gui_new_entry();
    gtk_widget_set_tooltip_text(entry, _("Page number"));
    g_object_set(G_OBJECT(entry), "margin", 5, NULL);
    gtk_container_add(GTK_CONTAINER(grid), entry);

    toolitem = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(toolitem), grid);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

    toolitem = gtk_tool_button_new(NULL, _("_Jump to"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolitem), "go-jump");
    gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem), _("Go To Page"));
    g_signal_connect(G_OBJECT(toolitem), "clicked",
		     G_CALLBACK(tg_gui_cb_goto_page), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

    toolitem = gtk_tool_button_new(NULL, _("_Back"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolitem), "go-previous");
    gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem), _("Get Previous Page"));
    g_signal_connect(G_OBJECT(toolitem), "clicked",
		     G_CALLBACK(tg_gui_cb_prev_page), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

    toolitem = gtk_tool_button_new(NULL, _("_Forward"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolitem), "go-next");
    gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem), _("Get Next Page"));
    g_signal_connect(G_OBJECT(toolitem), "clicked",
		     G_CALLBACK(tg_gui_cb_next_page), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

    toolitem = gtk_tool_button_new(NULL, _("_Home"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolitem), "go-home");
    gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem),
				_("Go to the home page"));
    g_signal_connect(G_OBJECT(toolitem), "clicked",
		     G_CALLBACK(tg_gui_cb_home), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

    toolitem = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolitem), _("_Play"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toolitem),
				  "media-playback-start");
    gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem), _("Toggles auto-paging"));
    g_signal_connect(G_OBJECT(toolitem), "toggled",
		     G_CALLBACK(tg_gui_cb_toggle_paging), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);

    /* FIXME */ /*
    toolitem = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(toolitem), _("100%"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(toolitem), _("Toggles zooming"));
    gui->zoombutton = GTK_WIDGET(toolitem);
    g_signal_connect(G_OBJECT(toolitem), "toggled",
		     G_CALLBACK(tg_gui_cb_zoom), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
    */

    gtk_widget_show_all(toolbar);
    
    return toolbar;
}

static void
tg_gui_init (TgGui *self)
{
}

static void
tg_gui_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    TgGui *self = TG_GUI (object);

    switch (property_id) {
	case PROP_CHANNEL_CHILDREN:
	    g_value_set_boxed (value, self->channel_children);
	    break;

	case PROP_CURRENT_CHANNEL:
	    g_value_set_string (value, self->current_channel);
	    break;

	case PROP_ZOOM_FACTOR:
	    g_value_set_int (value, currentview->zoom_factor);
	    break;

	case PROP_PAGING_ENABLED:
	    g_value_set_boolean (value, self->page_status);
	    break;

	case PROP_PAGING_INTERVAL:
	    g_value_set_int (value, self->page_msecs);
	    break;

	case PROP_CURRENT_PAGE_NUMBER:
	    g_value_set_int (value, currentview->page_nr);
	    break;

	case PROP_CURRENT_SUBPAGE_NUMBER:
	    g_value_set_int (value, currentview->subpage_nr);
	    break;

	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	    break;
    }
}

static void
tg_gui_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    TgGui *self = TG_GUI (object);
    TgChannel *channel;

    switch (property_id) {
	case PROP_CHANNEL_CHILDREN:
	    g_strfreev (self->channel_children);
	    self->channel_children = g_value_dup_boxed (value);
	    /* TODO: update channel list */
	    break;

	case PROP_CURRENT_CHANNEL:
	    g_free (self->current_channel);
	    self->current_channel = g_value_dup_string (value);
	    channel = tg_gui_channel_find_by_uuid (self->current_channel);
	    if (channel) {
		tg_gui_channel_select (channel);
		g_object_unref (G_OBJECT (channel));
	    } else {
		if (currentview->channel)
		    g_object_unref (G_OBJECT (currentview->channel));
		currentview->channel = NULL;
	    }
	    break;

	case PROP_ZOOM_FACTOR:
	    currentview->zoom_factor = g_value_get_int (value);
	    break;

	case PROP_PAGING_ENABLED:
	    self->page_status = g_value_get_boolean (value);
	    break;

	case PROP_PAGING_INTERVAL:
	    self->page_msecs = g_value_get_int (value);
	    break;

	case PROP_CURRENT_PAGE_NUMBER:
	    currentview->page_nr = g_value_get_int (value);
	    break;

	case PROP_CURRENT_SUBPAGE_NUMBER:
	    currentview->subpage_nr = g_value_get_int (value);
	    break;

	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	    break;
    }
}

static void
tg_gui_finalize (GObject *object)
{
    TgGui *gui = TG_GUI (object);

    g_clear_pointer (&gui->window, gtk_widget_destroy);
    g_clear_object (&gui->settings);
    g_clear_pointer (&gui->channel_children, g_strfreev);
    if (gui->channels != NULL) {
	g_slist_free_full (gui->channels, g_object_unref);
	gui->channels = NULL;
    }

    G_OBJECT_CLASS (tg_gui_parent_class)->finalize (object);
}

static void
tg_gui_class_init (TgGuiClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->get_property = tg_gui_get_property;
    gobject_class->set_property = tg_gui_set_property;
    gobject_class->finalize = tg_gui_finalize;

    g_object_class_install_property
	(gobject_class, PROP_CHANNEL_CHILDREN,
	 g_param_spec_boxed ("channel-children",
			     "Channel children",
			     "List of relative settings paths at which channels are stored",
			     G_TYPE_STRV,
			     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
	(gobject_class, PROP_CURRENT_CHANNEL,
	 g_param_spec_string ("current-channel",
			      "Current channel", "Current channel",
			      NULL,
			      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
	(gobject_class, PROP_ZOOM_FACTOR,
	 g_param_spec_int ("zoom-factor",
			   "Zoom factor",
			   "Page zoom factor.  Larger numbers produce larger text.",
			   1, 2, 1,
			   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
	(gobject_class, PROP_PAGING_ENABLED,
	 g_param_spec_boolean ("paging-enabled",
			       "Paging enabled",
			       "Automatically switch page at periodic intervals",
			       FALSE,
			       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
	(gobject_class, PROP_PAGING_INTERVAL,
	 g_param_spec_int ("paging-interval",
			   "Paging interval",
			   "Specifies the interval for the auto-pager, in milliseconds.",
			   1000, 60000, 12000,
			   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
	(gobject_class, PROP_CURRENT_PAGE_NUMBER,
	 g_param_spec_int ("current-page-number",
			   "Current page number", "Current page number",
			   -1, G_MAXINT, -1,
			   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
	(gobject_class, PROP_CURRENT_SUBPAGE_NUMBER,
	 g_param_spec_int ("current-subpage-number",
			   "Current subpage number", "Current subpage number",
			   -1, G_MAXINT, -1,
			   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/*******************************
 * return the app gui
 */
TgGui *
tg_gui_new (GtkApplication *app, GSettings *settings)
{
    GtkWidget *toolbar;
    GtkWidget *status_frame;
    GtkWidget *contents;
    GdkPixbuf *pixbuf;
    GError *error = NULL;

    gui = g_object_new (TG_TYPE_GUI, NULL);

    /* the app */
    gui->window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (gui->window),
			  _("TeleGNOME: Teletext for GNOME"));
    gtk_window_set_resizable (GTK_WINDOW (gui->window), FALSE);
    gui->accel_group = gtk_accel_group_new ();
    gtk_window_add_accel_group (GTK_WINDOW (gui->window), gui->accel_group);
    gui->grid = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (gui->window), gui->grid);

    gtk_widget_realize (GTK_WIDGET (gui->window));

    /* attach a keyboard event */
    g_signal_connect (G_OBJECT (gui->window), "key-press-event",
		      G_CALLBACK (tg_cb_keypress), NULL);
    
    toolbar = tg_gui_new_toolbar();
    gtk_grid_attach (GTK_GRID (gui->grid), toolbar, 0, 1, 2, 1);

    gui->channel_menu = gtk_application_get_menu_by_id (app, "channels");

    /* the view */
    currentview = tg_view_new();

    tg_view_set_error_handler(currentview, tg_gui_print_in_statusbar);

    gui->settings = g_object_ref (settings);
    g_settings_bind (gui->settings, "channel-children", gui, "channel-children",
		     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (gui->settings, "current-channel", gui, "current-channel",
		     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (gui->settings, "zoom-factor", gui, "zoom-factor",
		     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (gui->settings, "paging-enabled", gui, "paging-enabled",
		     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (gui->settings, "paging-interval", gui, "paging-interval",
		     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (gui->settings, "current-page-number", gui, "current-page-number",
		     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (gui->settings, "current-subpage-number", gui, "current-subpage-number",
		     G_SETTINGS_BIND_DEFAULT);

    /* all the contents */
    contents = tg_view_get_widget (currentview);
    gtk_widget_set_hexpand (contents, TRUE);
    gtk_widget_set_halign (contents, GTK_ALIGN_FILL);
    gtk_widget_set_vexpand (contents, TRUE);
    gtk_widget_set_valign (contents, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (gui->grid), contents, 0, 2, 2, 1);

    /* the progress and status bars */
    gui->progress_bar = gtk_progress_bar_new ();
    gtk_grid_attach (GTK_GRID (gui->grid), gui->progress_bar, 0, 3, 1, 1);
    status_frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (status_frame), GTK_SHADOW_IN);
    gui->status_bar = gtk_statusbar_new ();
    gtk_container_add (GTK_CONTAINER (status_frame), gui->status_bar);
    gtk_widget_set_hexpand (status_frame, TRUE);
    gtk_widget_set_halign (status_frame, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (gui->grid), status_frame, 1, 3, 1, 1);

    gtk_widget_show_all (gui->window);

    gui->kb_timer = -1;
    gui->kb_status = INPUT_NEW;

    gui->page_progress = 0;
    gui->page_timer = -1;

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gui->progress_bar), 0.0);

#if 0
    /* the zoom button */
    /* FIXME */
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(gui->zoombutton),
			      currentview->zoom_factor==1?"100%":"400%");
    if (currentview->zoom_factor==2)
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(gui->zoombutton), TRUE);
#endif

    /* g_print("Number: %d/%d\n", currentview->page_nr, currentview->subpage_nr); */

    gui->channels = NULL;

    tg_gui_refresh_channel_menu();

    /* FIXME: */
    /* set the current view, at elem 0 */
    currentview->channel = tg_gui_channel_find_by_uuid(gui->current_channel);

    tg_gui_update_title_bar();

    /* check if we are connected to a session manager. If so,
       load up the last page we were visiting. Otherwise,
       start with a logo */
    tg_gui_update_entry(0,0);
    error = NULL;
    pixbuf = gdk_pixbuf_new_from_resource (TG_LOGO_PIXMAP, &error);
    g_assert_no_error (error);
    tg_view_update_pixmap(currentview, pixbuf);
    g_object_unref(pixbuf);
    
    /* only auto-change to a page if it was saved the last time */

    if (currentview->page_nr >0 )
	gui->logo_timer = g_timeout_add (TG_LOGO_TIMEOUT, tg_gui_logo_timer,
					 NULL);
    else
	gui->logo_timer = -1;
    
    return gui;
}

GtkWidget *
tg_gui_get_window (TgGui *gui)
{
    return gui->window;
}


/*******************************
 * update the entry box with the current values of page & subpage
 */
int
tg_gui_update_entry (gint page_nr, gint subpage_nr)
{
    gchar full_num[10];
    
    if (subpage_nr > 0)
	sprintf(full_num,"%d/%d", page_nr, subpage_nr);
    else
	sprintf(full_num,"%d", page_nr);
    gtk_entry_set_text(GTK_ENTRY(gui->entry), full_num);
    
    return 0;
}

/*******************************
 * try to get the page, and do something smart if it fails
 * if redraw == TRUE, redraws whole app
 * (needed when zoom_factor has changed)
 */
void
tg_gui_get_the_page (gboolean redraw)
{
    /* hide the app */
    if (redraw)
	gtk_widget_hide(gui->window);

    /* stop the logo timer */
    if (gui->logo_timer != -1)
	g_source_remove(gui->logo_timer);
    gui->logo_timer = -1;

    if (currentview->channel)
	tg_view_update_page(currentview, &currentview->page_nr, &currentview->subpage_nr);

    tg_gui_update_entry(currentview->page_nr, currentview->subpage_nr);
    tg_gui_print_in_statusbar (NULL);

    if (redraw) 
      gtk_widget_show_all (gui->window);
}


/******************************* 
 * callbacks 
 */
void
tg_gui_activate_quit (GSimpleAction *action, GVariant *parameter,
		      gpointer data)
{
    /* free the channels */
    if (gui->channels != NULL) {
	g_slist_free_full(gui->channels, g_object_unref);
	gui->channels = NULL;
    }
    g_clear_pointer(&currentview, tg_view_free);

    /* get outta here ;) */
    g_application_quit(g_application_get_default());
}

void
tg_gui_activate_help_contents (GSimpleAction *action, GVariant *parameter,
			       gpointer data)
{
    GError *error;
    gboolean ret;

    error = NULL;
    ret = gtk_show_uri (gtk_widget_get_screen (gui->window), "ghelp:" PACKAGE,
			GDK_CURRENT_TIME, &error);
    if (!ret && error) {
	g_warning ("Error displaying help: %s", error->message);
	g_error_free (error);
    }
}

void
tg_gui_activate_about (GSimpleAction *action, GVariant *parameter,
		       gpointer data)
{
    static GtkWidget *about;
    const gchar *authors[]= { "Dirk-Jan C. Binnema <djcb@dds.nl>",
			      "Arjan Scherpenisse <acscherp@wins.uva.nl>",
			      "Colin Watson <cjwatson@debian.org>",
			      NULL    };

    if (about) {
	gdk_window_show(gtk_widget_get_window(about));
	gdk_window_raise(gtk_widget_get_window(about));
	return;
    }

    about = gtk_about_dialog_new();
    g_object_set(
	about,
	"program-name", PACKAGE,
	"version", VERSION,
	"copyright", "\xc2\xa9 1999, 2000 Dirk-Jan C. Binnema, "
		     "Arjan Scherpenisse; "
		     "\xc2\xa9 2008 Colin Watson",
	"comments", _("Teletext for GNOME"),
	"license", _("GNU General Public License, version 2 or later"),
	"website", "http://telegnome.sourceforge.net/",
	"authors", authors,
	"translator-credits", _("translator-credits"),
	NULL);

    gtk_window_set_transient_for(GTK_WINDOW(about), GTK_WINDOW(gui->window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(about), TRUE);

    g_signal_connect(about, "destroy", G_CALLBACK(gtk_widget_destroyed),
		     &about);
    g_signal_connect(about, "response", G_CALLBACK(gtk_widget_destroy),
		     NULL);

    gtk_widget_show(about);
}

static void
tg_gui_refresh_timer (void)
{
    gdouble perc = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(gui->progress_bar));

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gui->progress_bar), perc);

    if (gui->page_status) {
	g_source_remove(gui->page_timer);
	gui->page_timer = g_timeout_add(gui->page_msecs/100, tg_gui_pager_timer, NULL);
    }
    
    gui->page_progress =(int)((gui->page_msecs/100)*perc);
}

static void
tg_gui_prefs_close_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
    tg_gui_refresh_channel_menu();
    tg_gui_refresh_timer();
}

void
tg_gui_activate_preferences (GSimpleAction *action, GVariant *parameter,
			     gpointer data)
{
    tg_prefs_show(GTK_WINDOW(gui->window), G_CALLBACK(tg_gui_prefs_close_cb));
}

void
tg_gui_cb_next_page (GtkWidget* widget, gpointer data)
{	
    if (currentview->subpage_nr == 0) 
	currentview->page_nr++;
    else
	currentview->subpage_nr++;
    
    tg_gui_update_entry(currentview->page_nr, currentview->subpage_nr);
    tg_gui_get_the_page(FALSE); /* dont redraw */ 
}

void
tg_gui_cb_prev_page (GtkWidget* widget, gpointer data)
{	
    if (currentview->subpage_nr>0)
	currentview->subpage_nr--;
    if (currentview->subpage_nr==0)
	currentview->page_nr--;
    
    tg_gui_update_entry(currentview->page_nr, currentview->subpage_nr);
    tg_gui_get_the_page(FALSE);
}

void
tg_gui_cb_home (GtkWidget* widget, gpointer data)
{	
    currentview->subpage_nr=0;
    currentview->page_nr=100;
    
    tg_gui_update_entry(currentview->page_nr, currentview->subpage_nr);
    tg_gui_get_the_page(FALSE);
}

void
tg_gui_cb_goto_page (GtkWidget *widget, gpointer data)
{
    gui->kb_status = INPUT_NEW;
    if ( -1 == tg_http_get_page_entry (gtk_entry_get_text(GTK_ENTRY(gui->entry)))) {
	tg_gui_print_in_statusbar(_("Error in page entry"));
	return;
    }
    tg_gui_get_the_page(FALSE);
}


/*
 * handler for zoom button
 */
void 
tg_gui_cb_zoom (GtkWidget *widget, gpointer data)
{
    /* new: just toggle it on click */
    if (currentview->zoom_factor==1) {
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(gui->zoombutton), "400%");
	currentview->zoom_factor=2;
    } else if (currentview->zoom_factor==2) {
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(gui->zoombutton), "100%");
	currentview->zoom_factor=1;
    }		
    /* now, get the page with the new zoom settings */
    tg_gui_get_the_page(TRUE);
}

static gint 
tg_gui_keyboard_timer (gpointer g) 
{
    g_source_remove(gui->kb_timer);
    gui->kb_timer = -1;
    gui->kb_status = INPUT_NEW;
    return 0;
}

gint 
tg_cb_keypress (GtkWidget *widget, GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_KP_Enter) {
	tg_gui_cb_goto_page(NULL, NULL);
	tg_gui_update_entry(currentview->page_nr, currentview->subpage_nr);
	return 0;
    }
    
    /* g_print("keypress\n"); */
    if (!gtk_widget_is_focus(GTK_WIDGET(gui->entry)))
	gtk_widget_grab_focus(GTK_WIDGET(gui->entry));

    if (gui->kb_status == INPUT_NEW) {
	gtk_entry_set_text(GTK_ENTRY(gui->entry), "");
    }

    if (gui->kb_timer != -1)
	g_source_remove(gui->kb_timer);
    gui->kb_timer = g_timeout_add(TG_KB_TIMEOUT, tg_gui_keyboard_timer, NULL);
    gui->kb_status = INPUT_CONTINUED;
    return 0;
}
