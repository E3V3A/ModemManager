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

#ifndef MM_RAWIP_SERIAL_PORT_H
#define MM_RAWIP_SERIAL_PORT_H

#include <glib.h>
#include <glib-object.h>

#include "mm-serial-port.h"

#define MM_TYPE_RAWIP_SERIAL_PORT            (mm_rawip_serial_port_get_type ())
#define MM_RAWIP_SERIAL_PORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_RAWIP_SERIAL_PORT, MMRawipSerialPort))
#define MM_RAWIP_SERIAL_PORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_RAWIP_SERIAL_PORT, MMRawipSerialPortClass))
#define MM_IS_RAWIP_SERIAL_PORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_RAWIP_SERIAL_PORT))
#define MM_IS_RAWIP_SERIAL_PORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_RAWIP_SERIAL_PORT))
#define MM_RAWIP_SERIAL_PORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_RAWIP_SERIAL_PORT, MMRawipSerialPortClass))

#define MM_RAWIP_SERIAL_PORT_NET_DEVICE "rawip-net-device"

typedef struct _MMRawipSerialPort MMRawipSerialPort;
typedef struct _MMRawipSerialPortClass MMRawipSerialPortClass;
typedef struct _MMRawipSerialPortPrivate MMRawipSerialPortPrivate;

struct _MMRawipSerialPort {
    MMSerialPort parent;
    MMRawipSerialPortPrivate *priv;
};

struct _MMRawipSerialPortClass {
    MMSerialPortClass parent;
};

GType mm_rawip_serial_port_get_type (void);

MMRawipSerialPort *mm_rawip_serial_port_new (const gchar *name,
					     const gchar *net_device_name);

#endif /* MM_RAWIP_SERIAL_PORT_H */
