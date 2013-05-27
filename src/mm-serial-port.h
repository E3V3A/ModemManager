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
 * Copyright (C) 2008 - 2009 Novell, Inc.
 * Copyright (C) 2009 - 2010 Red Hat, Inc.
 */

#ifndef MM_SERIAL_PORT_H
#define MM_SERIAL_PORT_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "mm-port.h"

#define MM_TYPE_SERIAL_PORT            (mm_serial_port_get_type ())
#define MM_SERIAL_PORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_SERIAL_PORT, MMSerialPort))
#define MM_SERIAL_PORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_SERIAL_PORT, MMSerialPortClass))
#define MM_IS_SERIAL_PORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_SERIAL_PORT))
#define MM_IS_SERIAL_PORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_SERIAL_PORT))
#define MM_SERIAL_PORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_SERIAL_PORT, MMSerialPortClass))

#define MM_SERIAL_PORT_BAUD         "baud"
#define MM_SERIAL_PORT_BITS         "bits"
#define MM_SERIAL_PORT_PARITY       "parity"
#define MM_SERIAL_PORT_STOPBITS     "stopbits"
#define MM_SERIAL_PORT_RTS_CTS      "rts-cts"
#define MM_SERIAL_PORT_FD           "fd" /* Construct-only */
#define MM_SERIAL_PORT_FLASH_OK     "flash-ok" /* Construct-only */

typedef struct _MMSerialPort MMSerialPort;
typedef struct _MMSerialPortClass MMSerialPortClass;
typedef struct _MMSerialPortPrivate MMSerialPortPrivate;

typedef void (*MMSerialReopenFn) (MMSerialPort *port,
                                  GError *error,
                                  gpointer user_data);

typedef void (*MMSerialFlashFn)  (MMSerialPort *port,
                                  GError *error,
                                  gpointer user_data);

struct _MMSerialPort {
    MMPort parent;
    MMSerialPortPrivate *priv;
};

struct _MMSerialPortClass {
    MMPortClass parent;

    /* Called to configure the serial port fd after it's opened.  On error, should
     * return FALSE and set 'error' as appropriate.
     */
    gboolean (*config_fd)         (MMSerialPort *self, gint fd, GError **error);

    /* Called to setup data watchers */
    void     (*data_watch_enable) (MMSerialPort *self, gint fd);

    /* Called to configure the serial port after it's opened. Errors, if any,
     * should get ignored. */
    void     (*config)            (MMSerialPort *self);

    void     (*debug_log)         (MMSerialPort *self,
                                   const gchar *prefix,
                                   const gchar *buf,
                                   gsize len);

    /* Signals */
    void     (*forced_close)      (MMSerialPort *port);
};

GType mm_serial_port_get_type (void);

MMSerialPort *mm_serial_port_new (const gchar *name,
                                  MMPortType ptype);

/* Keep in mind that port open/close is refcounted, so ensure that
 * open/close calls are properly balanced.
 */

gboolean mm_serial_port_is_open           (MMSerialPort *self);

gboolean mm_serial_port_open              (MMSerialPort *self,
                                           GError  **error);

void     mm_serial_port_close             (MMSerialPort *self);

void     mm_serial_port_close_force       (MMSerialPort *self);

gboolean mm_serial_port_reopen            (MMSerialPort *self,
                                           guint32 reopen_time,
                                           MMSerialReopenFn callback,
                                           gpointer user_data);

gboolean mm_serial_port_flash             (MMSerialPort *self,
                                           guint32 flash_time,
                                           gboolean ignore_errors,
                                           MMSerialFlashFn callback,
                                           gpointer user_data);

void     mm_serial_port_flash_cancel      (MMSerialPort *self);

gboolean mm_serial_port_get_flash_ok      (MMSerialPort *self);


#endif /* MM_SERIAL_PORT_H */
