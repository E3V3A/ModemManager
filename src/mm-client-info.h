/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2014 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2014 Aleksander Morgado <aleksander@aleksander.es>
 *
 * Based on geoclue's GClueClientInfo.
 */

#ifndef MM_CLIENT_INFO_H
#define MM_CLIENT_INFO_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define MM_TYPE_CLIENT_INFO            (mm_client_info_get_type ())
#define MM_CLIENT_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_CLIENT_INFO, MMClientInfo))
#define MM_CLIENT_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_CLIENT_INFO, MMClientInfoClass))
#define MM_IS_CLIENT_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_CLIENT_INFO))
#define MM_IS_CLIENT_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_CLIENT_INFO))
#define MM_CLIENT_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_CLIENT_INFO, MMClientInfoClass))

typedef struct _MMClientInfo MMClientInfo;
typedef struct _MMClientInfoClass MMClientInfoClass;
typedef struct _MMClientInfoPrivate MMClientInfoPrivate;

struct _MMClientInfo {
    GObject parent;
    /*< private >*/
    MMClientInfoPrivate *priv;
};

struct _MMClientInfoClass {
    GObjectClass parent;
    /* signals */
    void (* peer_vanished)  (MMClientInfo *info);
};

GType         mm_client_info_get_type     (void);
void          mm_client_info_new          (const char          *bus_name,
                                           GDBusConnection     *connection,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data);
MMClientInfo *mm_client_info_new_finish   (GAsyncResult        *res,
                                           GError             **error);
MMClientInfo *mm_client_info_new_sync     (const char          *bus_name,
                                           GDBusConnection     *connection,
                                           GCancellable        *cancellable,
                                           GError             **error);
const gchar  *mm_client_info_get_bus_name (MMClientInfo        *info);
guint32       mm_client_info_get_user_id  (MMClientInfo        *info);

#endif /* MM_CLIENT_INFO_H */
