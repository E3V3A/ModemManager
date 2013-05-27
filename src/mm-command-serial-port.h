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
 * Copyright (C) 2009 - 2013 Red Hat, Inc.
 */

#ifndef MM_COMMAND_SERIAL_PORT_H
#define MM_COMMAND_SERIAL_PORT_H

#include <glib.h>
#include <glib-object.h>

#include "mm-serial-port.h"

#define MM_TYPE_COMMAND_SERIAL_PORT            (mm_command_serial_port_get_type ())
#define MM_COMMAND_SERIAL_PORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_COMMAND_SERIAL_PORT, MMCommandSerialPort))
#define MM_COMMAND_SERIAL_PORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_COMMAND_SERIAL_PORT, MMCommandSerialPortClass))
#define MM_IS_COMMAND_SERIAL_PORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_COMMAND_SERIAL_PORT))
#define MM_IS_COMMAND_SERIAL_PORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_COMMAND_SERIAL_PORT))
#define MM_COMMAND_SERIAL_PORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_COMMAND_SERIAL_PORT, MMCommandSerialPortClass))

#define MM_COMMAND_SERIAL_PORT_SEND_DELAY   "send-delay"
#define MM_COMMAND_SERIAL_PORT_SPEW_CONTROL "spew-control" /* Construct-only */

typedef struct _MMCommandSerialPort MMCommandSerialPort;
typedef struct _MMCommandSerialPortClass MMCommandSerialPortClass;
typedef struct _MMCommandSerialPortPrivate MMCommandSerialPortPrivate;

typedef void (*MMCommandSerialResponseFn) (MMCommandSerialPort *port,
                                           GByteArray *response,
                                           GError *error,
                                           gpointer user_data);

struct _MMCommandSerialPort {
    MMSerialPort parent;
    MMCommandSerialPortPrivate *priv;
};

struct _MMCommandSerialPortClass {
    MMSerialPortClass parent;

    /* Called for subclasses to parse unsolicited responses.  If any recognized
     * unsolicited response is found, it should be removed from the 'response'
     * byte array before returning.
     */
    void     (*parse_unsolicited) (MMCommandSerialPort *self,
                                   GByteArray *response);

    /* Called to parse the device's response to a command or determine if the
     * response was an error response.  If the response indicates an error, an
     * appropriate error should be returned in the 'error' argument.  The
     * function should return FALSE if there is not enough data yet to determine
     * the device's reply (whether success *or* error), and should return TRUE
     * when the device's response has been recognized and parsed.
     */
    gboolean (*parse_response)    (MMCommandSerialPort *self,
                                   GByteArray *response,
                                   GError **error);

    /* Called after parsing to allow the command response to be delivered to
     * it's callback to be handled.  Returns the # of bytes of the response
     * consumed.
     */
    gsize     (*handle_response)  (MMCommandSerialPort *self,
                                   GByteArray *response,
                                   GError *error,
                                   GCallback callback,
                                   gpointer callback_data);

    /* Signals */
    void (*buffer_full)           (MMCommandSerialPort *port,
                                   const GByteArray *buffer);

    void (*timed_out)             (MMCommandSerialPort *port,
                                   guint n_consecutive_replies);
};

GType mm_command_serial_port_get_type (void);

void     mm_command_serial_port_queue_command (MMCommandSerialPort *self,
                                               GByteArray *command,
                                               gboolean take_command,
                                               guint32 timeout_seconds,
                                               GCancellable *cancellable,
                                               MMCommandSerialResponseFn callback,
                                               gpointer user_data);

void     mm_command_serial_port_queue_command_cached (MMCommandSerialPort *self,
                                                      GByteArray *command,
                                                      gboolean take_command,
                                                      guint32 timeout_seconds,
                                                      GCancellable *cancellable,
                                                      MMCommandSerialResponseFn callback,
                                                      gpointer user_data);

#endif /* MM_COMMAND_SERIAL_PORT_H */
