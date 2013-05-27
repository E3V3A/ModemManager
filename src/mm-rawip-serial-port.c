/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundrawipion; either version 2 of the License, or
 * (rawip your option) any lrawiper version.
 *
 * This program is distributed in the hope thrawip it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2013 - Aleksander Morgado <aleksander@gnu.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include <ModemManager.h>
#include <mm-errors-types.h>

#include "mm-rawip-serial-port.h"
#include "mm-log.h"

G_DEFINE_TYPE (MMRawipSerialPort, mm_rawip_serial_port, MM_TYPE_SERIAL_PORT)

#define CHANNEL_BUFFER_SIZE 2048

enum {
    PROP_0,
    PROP_NET_DEVICE,
    LAST_PROP
};

struct _MMRawipSerialPortPrivate{
    /* Properties */
    gchar *net_device_name;

    /* TUN channel */
    GIOChannel *tun_channel;
    guint tun_channel_read_watch_id;
    guint tun_channel_write_watch_id;
    guint tun_channel_error_watch_id;
    guint8 *tun_channel_buffer;
    guint tun_channel_buffer_n;

    /* TTY channel */
    GIOChannel *tty_channel;
    guint tty_channel_read_watch_id;
    guint tty_channel_write_watch_id;
    guint tty_channel_error_watch_id;
    guint8 *tty_channel_buffer;
    guint tty_channel_buffer_n;
};

static void tty_write (MMRawipSerialPort *self);
static void tun_write (MMRawipSerialPort *self);

/*****************************************************************************/
/* Common */

static gboolean
error_available (GIOChannel *source,
                 GIOCondition condition,
                 gpointer data)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (data);
    const gchar *device;

    device = mm_port_get_device (MM_PORT (self));
    if (condition & G_IO_HUP)
        mm_dbg ("(%s) unexpected port hangup!", device);
    else if (condition & G_IO_ERR)
        mm_dbg ("(%s) unexpected port error!", device);
    else if (condition & G_IO_NVAL)
        mm_dbg ("(%s) unexpected port error: file descriptor not open!", device);
    else
        mm_dbg ("(%s) unexpected port conditition (%u)!", device, (guint) condition);

    mm_serial_port_close_force (MM_SERIAL_PORT (self));
    return FALSE;
}

/*****************************************************************************/
/* TUN channel handling */

static gboolean
tun_write_available (GIOChannel *source,
                     GIOCondition condition,
                     gpointer data)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (data);
    gsize i = 0;
    GIOStatus status;
    gsize write_count;

    g_source_remove (self->priv->tun_channel_write_watch_id);
    self->priv->tun_channel_write_watch_id = 0;

    if (!(condition & G_IO_OUT))
        return TRUE;

    if (!self->priv->tty_channel_buffer_n)
        return TRUE;

    do {
        gssize count;

        write_count = 0;
        count = CHANNEL_BUFFER_SIZE - self->priv->tun_channel_buffer_n;

        status = g_io_channel_write_chars (self->priv->tun_channel,
                                           (const gchar *)&self->priv->tty_channel_buffer[i],
                                           self->priv->tty_channel_buffer_n,
                                           &write_count,
                                           NULL);

        if (write_count > 0) {
            i += write_count;
            self->priv->tty_channel_buffer_n -= write_count;
        }
    } while (status == G_IO_STATUS_NORMAL && write_count > 0);

    /* TODO: ringbuffer saves us the memmove */
    if (self->priv->tty_channel_buffer_n > 0) {
        memmove (&self->priv->tty_channel_buffer[0],
                 &self->priv->tty_channel_buffer[i],
                 self->priv->tty_channel_buffer_n);

        /* Setup again */
        tun_write (self);
    }

    return TRUE;
}

static void
tun_write (MMRawipSerialPort *self)
{
    if (!self->priv->tty_channel_buffer_n || self->priv->tun_channel_write_watch_id != 0)
        return;

    /* If there is something in the TTY buffer, write in the TUN buffer */
    self->priv->tun_channel_write_watch_id = g_io_add_watch (self->priv->tun_channel,
                                                             G_IO_OUT,
                                                             tun_write_available,
                                                             self);
}

static gboolean
tun_read_available (GIOChannel *source,
                    GIOCondition condition,
                    gpointer data)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (data);
    GIOStatus status;
    gsize read_count;

    if (!(condition & G_IO_IN))
        return TRUE;

    do {
        gsize count;

        count = CHANNEL_BUFFER_SIZE - self->priv->tun_channel_buffer_n;

        status = g_io_channel_read_chars (self->priv->tun_channel,
                                          (gchar *)&self->priv->tun_channel_buffer[self->priv->tun_channel_buffer_n],
                                          count,
                                          &read_count,
                                          NULL);

        if (read_count > 0)
            self->priv->tun_channel_buffer_n += read_count;
    } while (status == G_IO_STATUS_NORMAL && read_count > 0);

    /* If something read, setup writing */
    if (self->priv->tun_channel_buffer_n > 0)
        tty_write (self);

    return TRUE;
}

static gboolean
create_tun_channel (MMRawipSerialPort *self,
                    GError **error)
{
    struct ifreq ifr;
    gint fd;

    /* open the clone device */
    fd = open ("/dev/net/tun", O_RDWR);
    if (fd < 0 ) {
        g_set_error (error,
                     MM_SERIAL_ERROR,
                     MM_SERIAL_ERROR_UNKNOWN,
                     "Couldn't open net/tun clone device");
        return FALSE;
    }

    /*
     * IFF_TUN: TUN device (no ethernet headers)
     * IFF_NO_PI: Do not provide packet information
     */
    memset (&ifr, 0, sizeof (ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    g_warn_if_fail (strlen (self->priv->net_device_name) < IFNAMSIZ);
    strncpy (ifr.ifr_name, self->priv->net_device_name, IFNAMSIZ);
    if (ioctl (fd, TUNSETIFF, (void *) &ifr) < 0) {
        g_set_error (error,
                     MM_SERIAL_ERROR,
                     MM_SERIAL_ERROR_UNKNOWN,
                     "Couldn't create TUN device: %s",
                     g_strerror (errno));
        close (fd);
        return FALSE;
    }

    /* Create GIOChannel for the TUN device */
    g_warn_if_fail (self->priv->tun_channel == NULL);
    g_warn_if_fail (self->priv->tun_channel_read_watch_id == 0);
    self->priv->tun_channel = g_io_channel_unix_new (fd);
    g_io_channel_set_close_on_unref (self->priv->tun_channel, TRUE);
    g_io_channel_set_encoding (self->priv->tun_channel, NULL, NULL);
    self->priv->tun_channel_error_watch_id = g_io_add_watch (self->priv->tun_channel,
                                                             G_IO_NVAL | G_IO_ERR | G_IO_HUP,
                                                             error_available,
                                                             self);
    self->priv->tun_channel_read_watch_id = g_io_add_watch (self->priv->tun_channel,
                                                            G_IO_IN,
                                                            tun_read_available,
                                                            self);

    return TRUE;
}

static void
remove_tun_channel (MMRawipSerialPort *self)
{
    if (self->priv->tun_channel_error_watch_id != 0) {
        g_source_remove (self->priv->tun_channel_error_watch_id);
        self->priv->tun_channel_error_watch_id = 0;
    }

    if (self->priv->tun_channel_read_watch_id != 0) {
        g_source_remove (self->priv->tun_channel_read_watch_id);
        self->priv->tun_channel_read_watch_id = 0;
    }

    if (self->priv->tun_channel_write_watch_id != 0) {
        g_source_remove (self->priv->tun_channel_write_watch_id);
        self->priv->tun_channel_write_watch_id = 0;
    }

    if (self->priv->tun_channel) {
        g_io_channel_unref (self->priv->tun_channel);
        self->priv->tun_channel = NULL;
    }
}

/*****************************************************************************/
/* TTY channel handling */

static gboolean
tty_write_available (GIOChannel *source,
                     GIOCondition condition,
                     gpointer data)
{
    GIOStatus status;
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (data);
    gsize i = 0;
    gsize write_count;

    g_source_remove (self->priv->tty_channel_write_watch_id);
    self->priv->tty_channel_write_watch_id = 0;

    if (!(condition & G_IO_OUT))
        return TRUE;

    if (!self->priv->tun_channel_buffer_n)
        return TRUE;

    do {
        gssize count;

        write_count = 0;
        count = CHANNEL_BUFFER_SIZE - self->priv->tty_channel_buffer_n;

        status = g_io_channel_write_chars (self->priv->tty_channel,
                                           (const gchar *)&self->priv->tun_channel_buffer[i],
                                           self->priv->tun_channel_buffer_n,
                                           &write_count,
                                           NULL);

        if (write_count > 0) {
            i += write_count;
            self->priv->tun_channel_buffer_n -= write_count;
        }
    } while (status == G_IO_STATUS_NORMAL && write_count > 0);

    /* TODO: ringbuffer saves us the memmove */
    if (self->priv->tun_channel_buffer_n > 0) {
        memmove (&self->priv->tun_channel_buffer[0],
                 &self->priv->tun_channel_buffer[i],
                 self->priv->tun_channel_buffer_n);

        /* Setup again */
        tty_write (self);
    }

    return TRUE;
}

static void
tty_write (MMRawipSerialPort *self)
{
    if (!self->priv->tun_channel_buffer_n || self->priv->tty_channel_write_watch_id != 0)
        return;

    /* If there is something in the TUN buffer, write in the TTY buffer */
    self->priv->tty_channel_write_watch_id = g_io_add_watch (self->priv->tty_channel,
                                                             G_IO_OUT,
                                                             tty_write_available,
                                                             self);
}

static gboolean
tty_read_available (GIOChannel *source,
                    GIOCondition condition,
                    gpointer data)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (data);
    GIOStatus status;
    gsize read_count;

    if (!(condition & G_IO_IN))
        return TRUE;

    do {
        gsize count;

        count = CHANNEL_BUFFER_SIZE - self->priv->tty_channel_buffer_n;

        status = g_io_channel_read_chars (self->priv->tty_channel,
                                          (gchar *)&self->priv->tty_channel_buffer[self->priv->tty_channel_buffer_n],
                                          count,
                                          &read_count,
                                          NULL);

        if (read_count > 0)
            self->priv->tty_channel_buffer_n += read_count;
    } while (status == G_IO_STATUS_NORMAL && read_count > 0);

    /* If something read, setup writing */
    if (self->priv->tty_channel_buffer_n > 0)
        tun_write (self);

    return TRUE;
}

static void
create_tty_channel (MMRawipSerialPort *self,
                    gint fd)
{
    /* Create GIOChannel for the TTY device */
    g_warn_if_fail (self->priv->tty_channel == NULL);
    g_warn_if_fail (self->priv->tty_channel_read_watch_id == 0);
    self->priv->tty_channel = g_io_channel_unix_new (fd);
    /* Don't close the input FD ourselves, it's done by MMSerialPort */
    g_io_channel_set_encoding (self->priv->tty_channel, NULL, NULL);
    self->priv->tty_channel_error_watch_id = g_io_add_watch (self->priv->tty_channel,
                                                             G_IO_NVAL | G_IO_ERR | G_IO_HUP,
                                                             error_available,
                                                             self);
    self->priv->tty_channel_read_watch_id = g_io_add_watch (self->priv->tty_channel,
                                                            G_IO_IN,
                                                            tty_read_available,
                                                            self);
}

static void
remove_tty_channel (MMRawipSerialPort *self)
{
    if (self->priv->tty_channel_error_watch_id != 0) {
        g_source_remove (self->priv->tty_channel_error_watch_id);
        self->priv->tty_channel_error_watch_id = 0;
    }

    if (self->priv->tty_channel_read_watch_id != 0) {
        g_source_remove (self->priv->tty_channel_read_watch_id);
        self->priv->tty_channel_read_watch_id = 0;
    }

    if (self->priv->tty_channel_write_watch_id != 0) {
        g_source_remove (self->priv->tty_channel_write_watch_id);
        self->priv->tty_channel_write_watch_id = 0;
    }

    if (self->priv->tty_channel) {
        g_io_channel_unref (self->priv->tty_channel);
        self->priv->tty_channel = NULL;
    }
}

/*****************************************************************************/

static gboolean
data_watch_enable (MMSerialPort *_self,
                   gint fd,
                   GError **error)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (_self);

    /* Enabling... */
    if (fd >= 0) {
        if (!create_tun_channel (self, error))
            return FALSE;
        create_tty_channel (self, fd);
        return TRUE;
    }

    /* Disabling */
    remove_tun_channel (self);
    remove_tty_channel (self);
    return TRUE;
}

/*****************************************************************************/

MMRawipSerialPort *
mm_rawip_serial_port_new (const gchar *name,
                          const gchar *net_device_name)
{
    return MM_RAWIP_SERIAL_PORT (g_object_new (MM_TYPE_RAWIP_SERIAL_PORT,
                                               MM_PORT_DEVICE, name,
                                               MM_PORT_SUBSYS, MM_PORT_SUBSYS_TTY,
                                               MM_PORT_TYPE, MM_PORT_TYPE_RAWIP,
                                               MM_RAWIP_SERIAL_PORT_NET_DEVICE, net_device_name,
                                               NULL));
}

static void
mm_rawip_serial_port_init (MMRawipSerialPort *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MM_TYPE_RAWIP_SERIAL_PORT, MMRawipSerialPortPrivate);
    self->priv->tun_channel_buffer = g_malloc (CHANNEL_BUFFER_SIZE);
    self->priv->tty_channel_buffer = g_malloc (CHANNEL_BUFFER_SIZE);
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (object);

    switch (prop_id) {
    case PROP_NET_DEVICE:
        /* Construct only */
        self->priv->net_device_name = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (object);

    switch (prop_id) {
    case PROP_NET_DEVICE:
        g_value_set_string (value, self->priv->net_device_name);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (object);

    remove_tun_channel (self);
    remove_tty_channel (self);

    G_OBJECT_CLASS (mm_rawip_serial_port_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (object);

    g_free (self->priv->tun_channel_buffer);
    g_free (self->priv->tty_channel_buffer);
    g_free (self->priv->net_device_name);

    G_OBJECT_CLASS (mm_rawip_serial_port_parent_class)->finalize (object);
}

static void
mm_rawip_serial_port_class_init (MMRawipSerialPortClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMSerialPortClass *serial_port_class = MM_SERIAL_PORT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMRawipSerialPortPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;
    object_class->finalize = finalize;
    serial_port_class->debug_log = NULL;
    serial_port_class->config = NULL;
    serial_port_class->data_watch_enable = data_watch_enable;

    g_object_class_install_property
        (object_class, PROP_NET_DEVICE,
         g_param_spec_string (MM_RAWIP_SERIAL_PORT_NET_DEVICE,
                              "Net device",
                              "Net (TUN) device name",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}
