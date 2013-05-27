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

#include "mm-rawip-serial-port.h"
#include "mm-log.h"

G_DEFINE_TYPE (MMRawipSerialPort, mm_rawip_serial_port, MM_TYPE_SERIAL_PORT)

enum {
    PROP_0,
    PROP_NET_DEVICE,
    LAST_PROP
};

struct _MMRawipSerialPortPrivate{
    /* Properties */
    gchar *net_device_name;
};

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
finalize (GObject *object)
{
    MMRawipSerialPort *self = MM_RAWIP_SERIAL_PORT (object);

    g_free (self->priv->net_device_name);

    G_OBJECT_CLASS (mm_rawip_serial_port_parent_class)->finalize (object);
}

static void
mm_rawip_serial_port_class_init (MMRawipSerialPortClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMSerialPortClass *port_class = MM_SERIAL_PORT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMRawipSerialPortPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize = finalize;

    port_class->parse_unsolicited = NULL;
    port_class->parse_response = NULL;
    port_class->handle_response = NULL;
    port_class->debug_log = NULL;
    port_class->config = NULL;

    g_object_class_install_property
        (object_class, PROP_NET_DEVICE,
         g_param_spec_string (MM_RAWIP_SERIAL_PORT_NET_DEVICE,
                              "Net device",
                              "Net (TUN) device name",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}
