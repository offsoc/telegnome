/*
 * channel.h
 * A channel
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

#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TG_TYPE_CHANNEL             (tg_channel_get_type ())
G_DECLARE_FINAL_TYPE (TgChannel, tg_channel, TG, CHANNEL, GObject)

GType tg_channel_get_type (void);

TgChannel *tg_channel_new (const gchar *uuid, const gchar *first_property_name,
			   ...);

GSettings *tg_channel_get_settings (TgChannel *channel);

G_END_DECLS

#endif
