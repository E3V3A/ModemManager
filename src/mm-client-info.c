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
 * Based on geoclue's MMClientInfo.
 */

#include <pwd.h>

#include "mm-client-info.h"

static void async_initable_iface_init (GAsyncInitableIface *iface);
static void initable_iface_init       (GInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (MMClientInfo, mm_client_info, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

enum {
    PROP_0,
    PROP_PEER,
    PROP_CONNECTION,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

enum {
    PEER_VANISHED,
    SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

struct _MMClientInfoPrivate {
    gchar *bus_name;
    GDBusConnection *connection;
    guint watch_id;
    guint32 user_id;
};

/*****************************************************************************/

const gchar *
mm_client_info_get_bus_name (MMClientInfo *info)
{
    g_return_val_if_fail (MM_IS_CLIENT_INFO (info), NULL);

    return info->priv->bus_name;
}

guint32
mm_client_info_get_user_id (MMClientInfo *info)
{
    g_return_val_if_fail (MM_IS_CLIENT_INFO (info), 0);

    return info->priv->user_id;
}

/*****************************************************************************/

static gboolean
init_finish (GAsyncInitable  *initable,
             GAsyncResult    *result,
             GError         **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);
}

static void
name_vanished (GDBusConnection *connection,
               const gchar     *name,
               gpointer         user_data)
{
    g_signal_emit (MM_CLIENT_INFO (user_data), signals[PEER_VANISHED], 0);
}

static void
call_ready (GDBusConnection    *connection,
            GAsyncResult       *res,
            GSimpleAsyncResult *simple)
{
    MMClientInfo *self;
    GError *error = NULL;
    GVariant *results = NULL;

    self = MM_CLIENT_INFO (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));

    results = g_dbus_connection_call_finish (connection, res, &error);
    if (!results) {
        g_simple_async_result_take_error (simple, error);
    } else {
        /* Read uid */
        g_assert (g_variant_n_children (results) > 0);
        g_variant_get_child (results, 0, "u", &self->priv->user_id);
        g_variant_unref (results);

        /* Setup name watcher */
        self->priv->watch_id = g_bus_watch_name_on_connection (self->priv->connection,
                                                               self->priv->bus_name,
                                                               G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                               NULL,
                                                               name_vanished,
                                                               self,
                                                               NULL);
        g_simple_async_result_set_op_res_gboolean (simple, TRUE);
    }

    g_simple_async_result_complete (simple);
    g_object_unref (simple);
    g_object_unref (self);
}

static void
init_async (GAsyncInitable      *initable,
            int                  io_priority,
            GCancellable        *cancellable,
            GAsyncReadyCallback  callback,
            gpointer             user_data)
{
    MMClientInfo *self = MM_CLIENT_INFO (initable);
    GSimpleAsyncResult *simple;

    simple = g_simple_async_result_new (G_OBJECT (initable),
                                        callback,
                                        user_data,
                                        init_async);

    g_dbus_connection_call (self->priv->connection,
                            "org.freedesktop.DBus",
                            "/org/freedesktop/DBus",
                            "org.freedesktop.DBus",
                            "GetConnectionUnixUser",
                            g_variant_new ("(s)", self->priv->bus_name),
                            NULL, /* don't check reply type */
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            cancellable,
                            (GAsyncReadyCallback)call_ready,
                            simple);
}

/*****************************************************************************/

static gboolean
init (GInitable     *initable,
      GCancellable  *cancellable,
      GError       **error)
{
    MMClientInfo *self = MM_CLIENT_INFO (initable);
    GVariant *results;

    results = g_dbus_connection_call_sync (self->priv->connection,
                                           "org.freedesktop.DBus",
                                           "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus",
                                           "GetConnectionUnixUser",
                                           g_variant_new ("(s)", self->priv->bus_name),
                                           NULL, /* don't check reply type */
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           cancellable,
                                           error);
    if (!results)
        return FALSE;

    /* Read uid */
    g_assert (g_variant_n_children (results) > 0);
    g_variant_get_child (results, 0, "u", &self->priv->user_id);
    g_variant_unref (results);

    /* Setup name watcher */
    self->priv->watch_id = g_bus_watch_name_on_connection (self->priv->connection,
                                                           self->priv->bus_name,
                                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                           NULL,
                                                           name_vanished,
                                                           self,
                                                           NULL);
    return TRUE;
}

/*****************************************************************************/

MMClientInfo *
mm_client_info_new_finish (GAsyncResult *res,
                           GError      **error)
{
    GObject *client;
    GObject *source;

    source = g_async_result_get_source_object (res);
    client = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, error);
    g_object_unref (source);

    if (!source)
        return NULL;

    return MM_CLIENT_INFO (client);
}

void
mm_client_info_new (const gchar         *bus_name,
                    GDBusConnection     *connection,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    g_async_initable_new_async (MM_TYPE_CLIENT_INFO,
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                callback,
                                user_data,
                                "bus-name",   bus_name,
                                "connection", connection,
                                NULL);
}

/*****************************************************************************/

MMClientInfo *
mm_client_info_new_sync (const gchar      *bus_name,
                         GDBusConnection  *connection,
                         GCancellable     *cancellable,
                         GError          **error)
{
    return (MMClientInfo *) g_initable_new (MM_TYPE_CLIENT_INFO,
                                            cancellable,
                                            error,
                                            "bus-name",   bus_name,
                                            "connection", connection,
                                            NULL);
}

/*****************************************************************************/

static void
mm_client_info_init (MMClientInfo *info)
{
    info->priv = G_TYPE_INSTANCE_GET_PRIVATE (info, MM_TYPE_CLIENT_INFO, MMClientInfoPrivate);
}

static void
finalize (GObject *object)
{
    MMClientInfoPrivate *priv = MM_CLIENT_INFO (object)->priv;

    g_free (priv->bus_name);

    G_OBJECT_CLASS (mm_client_info_parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
    MMClientInfoPrivate *priv = MM_CLIENT_INFO (object)->priv;

    if (priv->watch_id != 0) {
        g_bus_unwatch_name (priv->watch_id);
        priv->watch_id = 0;
    }

    g_clear_object (&priv->connection);

    G_OBJECT_CLASS (mm_client_info_parent_class)->dispose (object);
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    MMClientInfo *info = MM_CLIENT_INFO (object);

    switch (prop_id) {
    case PROP_PEER:
        g_value_set_string (value, info->priv->bus_name);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, info->priv->connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    MMClientInfo *info = MM_CLIENT_INFO (object);

    switch (prop_id) {
    case PROP_PEER:
        info->priv->bus_name = g_value_dup_string (value);
        break;
    case PROP_CONNECTION:
        info->priv->connection = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
    iface->init_async = init_async;
    iface->init_finish = init_finish;
}

static void
initable_iface_init (GInitableIface *iface)
{
    iface->init = init;
}

static void
mm_client_info_class_init (MMClientInfoClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = finalize;
    object_class->dispose = dispose;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    g_type_class_add_private (object_class, sizeof (MMClientInfoPrivate));

    properties[PROP_PEER] = g_param_spec_string ("bus-name",
                                                 "BusName",
                                                 "Bus name of client",
                                                 NULL,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_property (object_class, PROP_PEER, properties[PROP_PEER]);

    properties[PROP_CONNECTION] = g_param_spec_object ("connection",
                                                       "Connection",
                                                       "DBus Connection",
                                                       G_TYPE_DBUS_CONNECTION,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_property (object_class, PROP_CONNECTION, properties[PROP_CONNECTION]);

    signals[PEER_VANISHED] =
        g_signal_new ("peer-vanished",
                      MM_TYPE_CLIENT_INFO,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MMClientInfoClass, peer_vanished),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0,
                      G_TYPE_NONE);
}
