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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <ModemManager.h>
#include <mm-errors-types.h>

#include "mm-command-serial-port.h"
#include "libqcdm/src/com.h"
#include "libqcdm/src/utils.h"
#include "libqcdm/src/errors.h"
#include "mm-log.h"

G_DEFINE_TYPE (MMCommandSerialPort, mm_command_serial_port, MM_TYPE_SERIAL_PORT)

#define SERIAL_BUF_SIZE 2048

enum {
    PROP_0,
    PROP_SEND_DELAY,
    PROP_SPEW_CONTROL,
    PROP_LAST
};

enum {
  SIGNAL_BUFFER_FULL,
  SIGNAL_TIMED_OUT,
  SIGNAL_LAST
};

struct _MMCommandSerialPortPrivate {
    guint64 send_delay;
    gboolean spew_control;

    gint fd;
    GHashTable *reply_cache;
    GIOChannel *channel;
    GQueue *queue;
    GByteArray *response;

    guint queue_id;
    guint watch_id;
    guint timeout_id;

    GCancellable *cancellable;
    gulong cancellable_id;

    guint n_consecutive_timeouts;

    guint connected_id;
};

typedef struct {
    GByteArray *command;
    guint32 idx;
    guint32 eagain_count;
    gboolean started;
    gboolean done;
    GCallback callback;
    gpointer user_data;
    guint32 timeout;
    gboolean cached;
    GCancellable *cancellable;
} MMQueueData;

static gboolean queue_process  (gpointer data);
static void     channel_enable (MMCommandSerialPort *self, gboolean enable);

static guint signals[SIGNAL_LAST] = { 0 };

/*****************************************************************************/

static void
serial_debug (MMCommandSerialPort *self, const char *prefix, const char *buf, gsize len)
{
    g_return_if_fail (len > 0);

    if (MM_SERIAL_PORT_GET_CLASS (self)->debug_log)
        MM_SERIAL_PORT_GET_CLASS (self)->debug_log (MM_SERIAL_PORT (self), prefix, buf, len);
}

/*****************************************************************************/

static gboolean
process_command (MMCommandSerialPort *self,
                 MMQueueData *info,
                 GError **error)
{
    const guint8 *p;
    gint status, expected_status, send_len;

    if (!self->priv->channel) {
        g_set_error_literal (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_SEND_FAILED,
                             "Sending command failed: device is not enabled");
        return FALSE;
    }

    if (mm_port_get_connected (MM_PORT (self))) {
        g_set_error_literal (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_SEND_FAILED,
                             "Sending command failed: device is connected");
        return FALSE;
    }

    /* Only print command the first time */
    if (info->started == FALSE) {
        info->started = TRUE;
        serial_debug (self, "-->", (const char *) info->command->data, info->command->len);
    }

    if (self->priv->send_delay == 0) {
        /* Send the whole command in one write */
        send_len = expected_status = info->command->len;
        p = info->command->data;
    } else {
        /* Send just one byte of the command */
        send_len = expected_status = 1;
        p = &info->command->data[info->idx];
    }

    /* Send a single byte of the command */
    errno = 0;
    status = write (g_io_channel_unix_get_fd (self->priv->channel), p, send_len);
    if (status > 0)
        info->idx += status;
    else {
        /* Error or no bytes written */
        if (errno == EAGAIN || status == 0) {
            info->eagain_count--;
            if (info->eagain_count <= 0) {
                /* If we reach the limit of EAGAIN errors, treat as a timeout error. */
                self->priv->n_consecutive_timeouts++;
                g_signal_emit (self, signals[SIGNAL_TIMED_OUT], 0, self->priv->n_consecutive_timeouts);

                g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_SEND_FAILED,
                             "Sending command failed: '%s'", strerror (errno));
                return FALSE;
            }
        } else {
            g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_SEND_FAILED,
                         "Sending command failed: '%s'", strerror (errno));
            return FALSE;
        }
    }

    if (info->idx >= info->command->len)
        info->done = TRUE;

    return TRUE;
}

static void
set_cached_reply (MMCommandSerialPort *self,
                  const GByteArray *command,
                  const GByteArray *response)
{
    g_return_if_fail (MM_IS_COMMAND_SERIAL_PORT (self));
    g_return_if_fail (command != NULL);

    if (response) {
        GByteArray *cmd_copy;
        GByteArray *rsp_copy;

        cmd_copy = g_byte_array_sized_new (command->len);
        g_byte_array_append (cmd_copy, command->data, command->len);
        rsp_copy = g_byte_array_sized_new (response->len);
        g_byte_array_append (rsp_copy, response->data, response->len);
        g_hash_table_insert (self->priv->reply_cache, cmd_copy, rsp_copy);
    } else
        g_hash_table_remove (self->priv->reply_cache, command);
}

static const GByteArray *
get_cached_reply (MMCommandSerialPort *self,
                  GByteArray *command)
{
    return (const GByteArray *) g_hash_table_lookup (self->priv->reply_cache, command);
}

static void
schedule_queue_process (MMCommandSerialPort *self,
                        guint timeout_ms)
{
    if (self->priv->timeout_id) {
        /* A command is already in progress */
        return;
    }

    if (self->priv->queue_id) {
        /* Already scheduled */
        return;
    }

    if (timeout_ms)
        self->priv->queue_id = g_timeout_add (timeout_ms, queue_process, self);
    else
        self->priv->queue_id = g_idle_add (queue_process, self);
}

static gsize
real_handle_response (MMCommandSerialPort *self,
                      GByteArray *response,
                      GError *error,
                      GCallback callback,
                      gpointer callback_data)
{
    MMCommandSerialResponseFn response_callback = (MMCommandSerialResponseFn) callback;

    response_callback (self, response, error, callback_data);
    return response->len;
}

static void
got_response (MMCommandSerialPort *self,
              GError *error)
{
    MMQueueData *info;
    gsize consumed;

    consumed = self->priv->response->len;

    if (self->priv->timeout_id) {
        g_source_remove (self->priv->timeout_id);
        self->priv->timeout_id = 0;
    }

    if (self->priv->cancellable_id) {
        g_assert (self->priv->cancellable != NULL);
        g_cancellable_disconnect (self->priv->cancellable,
                                  self->priv->cancellable_id);
        self->priv->cancellable_id = 0;
    }

    g_clear_object (&self->priv->cancellable);

    info = (MMQueueData *) g_queue_pop_head (self->priv->queue);
    if (info) {
        if (info->cached && !error)
            set_cached_reply (self, info->command, self->priv->response);

        if (info->callback) {
            g_warn_if_fail (MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->handle_response != NULL);
            consumed = MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->handle_response (self,
                                                                                 self->priv->response,
                                                                                 error,
                                                                                 info->callback,
                                                                                 info->user_data);
        }

        g_clear_object (&info->cancellable);
        g_byte_array_free (info->command, TRUE);
        g_slice_free (MMQueueData, info);
    }

    if (error)
        g_error_free (error);

    if (consumed)
        g_byte_array_remove_range (self->priv->response, 0, consumed);
    if (!g_queue_is_empty (self->priv->queue))
        schedule_queue_process (self, 0);
}

static gboolean
timed_out (gpointer data)
{
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (data);
    GError *error;

    self->priv->timeout_id = 0;

    /* Update number of consecutive timeouts found */
    self->priv->n_consecutive_timeouts++;

    error = g_error_new_literal (MM_SERIAL_ERROR,
                                 MM_SERIAL_ERROR_RESPONSE_TIMEOUT,
                                 "Serial command timed out");

    /* FIXME: This is not completely correct - if the response finally arrives and there's
       some other command waiting for response right now, the other command will
       get the output of the timed out command. Not sure what to do here. */
    got_response (self, error);

    /* Emit a timed out signal, used by upper layers to identify a disconnected
     * serial port */
    g_signal_emit (self, signals[SIGNAL_TIMED_OUT], 0, self->priv->n_consecutive_timeouts);

    return FALSE;
}

static void
response_wait_cancelled (GCancellable *cancellable,
                         MMCommandSerialPort *self)
{
    GError *error;

    /* We don't want to call disconnect () while in the signal handler */
    self->priv->cancellable_id = 0;

    error = g_error_new_literal (MM_CORE_ERROR,
                                 MM_CORE_ERROR_CANCELLED,
                                 "Waiting for the reply cancelled");

    /* FIXME: This is not completely correct - if the response finally arrives and there's
       some other command waiting for response right now, the other command will
       get the output of the cancelled command. Not sure what to do here. */
    got_response (self, error);
}

static gboolean
queue_process (gpointer data)
{
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (data);
    MMQueueData *info;
    GError *error = NULL;

    self->priv->queue_id = 0;

    info = (MMQueueData *) g_queue_peek_head (self->priv->queue);
    if (!info)
        return FALSE;

    if (info->cached) {
        const GByteArray *cached;

        cached = get_cached_reply (self, info->command);
        if (cached) {
            /* Ensure the response array is fully empty before setting the
             * cached response.  */
            if (self->priv->response->len > 0) {
                mm_warn ("(%s) response array is not empty when using cached "
                         "reply, cleaning up %u bytes",
                         mm_port_get_device (MM_PORT (self)),
                         self->priv->response->len);
                g_byte_array_set_size (self->priv->response, 0);
            }

            g_byte_array_append (self->priv->response, cached->data, cached->len);
            got_response (self, NULL);
            return FALSE;
        }
    }

    if (process_command (self, info, &error)) {
        if (info->done) {
            /* setup the cancellable so that we can stop waiting for a response */
            if (info->cancellable) {
                self->priv->cancellable = g_object_ref (info->cancellable);
                self->priv->cancellable_id = (g_cancellable_connect (
                                                  info->cancellable,
                                                  (GCallback) response_wait_cancelled,
                                                  self,
                                                  NULL));
                if (!self->priv->cancellable_id) {
                    error = g_error_new (MM_CORE_ERROR,
                                         MM_CORE_ERROR_CANCELLED,
                                         "Won't wait for the reply");
                    got_response (self, error);
                    return FALSE;
                }
            }

            /* If the command is finished being sent, schedule the timeout */
            self->priv->timeout_id = g_timeout_add_seconds (info->timeout, timed_out, self);
        } else {
            /* Schedule the next byte of the command to be sent */
            schedule_queue_process (self, self->priv->send_delay / 1000);
        }
    } else
        got_response (self, error);

    return FALSE;
}

/*****************************************************************************/

static gboolean
parse_response (MMCommandSerialPort *self,
                GByteArray *response,
                GError **error)
{
    if (MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->parse_unsolicited)
        MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->parse_unsolicited (self, response);

    g_return_val_if_fail (MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->parse_response, FALSE);
    return MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->parse_response (self, response, error);
}

static gboolean
data_available (GIOChannel *source,
                GIOCondition condition,
                gpointer data)
{
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (data);
    char buf[SERIAL_BUF_SIZE + 1];
    gsize bytes_read;
    GIOStatus status;
    MMQueueData *info;
    const char *device;

    if (condition & G_IO_HUP) {
        device = mm_port_get_device (MM_PORT (self));
        mm_dbg ("(%s) unexpected port hangup!", device);

        if (self->priv->response->len)
            g_byte_array_remove_range (self->priv->response, 0, self->priv->response->len);
        mm_serial_port_close_force (MM_SERIAL_PORT (self));
        return FALSE;
    }

    if (condition & G_IO_ERR) {
        if (self->priv->response->len)
            g_byte_array_remove_range (self->priv->response, 0, self->priv->response->len);
        return TRUE;
    }

    /* Don't read any input if the current command isn't done being sent yet */
    info = g_queue_peek_nth (self->priv->queue, 0);
    if (info && (info->started == TRUE) && (info->done == FALSE))
        return TRUE;

    do {
        GError *err = NULL;

        bytes_read = 0;
        status = g_io_channel_read_chars (source, buf, SERIAL_BUF_SIZE, &bytes_read, &err);
        if (status == G_IO_STATUS_ERROR) {
            if (err && err->message) {
                mm_warn ("(%s): read error: %s",
                         mm_port_get_device (MM_PORT (self)),
                         err->message);
            }
            g_clear_error (&err);
        }

        /* If no bytes read, just let g_io_channel wait for more data */
        if (bytes_read == 0)
            break;

        g_assert (bytes_read > 0);
        serial_debug (self, "<--", buf, bytes_read);
        g_byte_array_append (self->priv->response, (const guint8 *) buf, bytes_read);

        /* Make sure the response doesn't grow too long */
        if ((self->priv->response->len > SERIAL_BUF_SIZE) && self->priv->spew_control) {
            /* Notify listeners and then trim the buffer */
            g_signal_emit (self, signals[SIGNAL_BUFFER_FULL], 0, self->priv->response);
            g_byte_array_remove_range (self->priv->response, 0, (SERIAL_BUF_SIZE / 2));
        }

        if (parse_response (self, self->priv->response, &err)) {
            /* Reset number of consecutive timeouts only here */
            self->priv->n_consecutive_timeouts = 0;
            got_response (self, err);
        }
    } while (   (bytes_read == SERIAL_BUF_SIZE || status == G_IO_STATUS_AGAIN)
             && (self->priv->watch_id > 0));

    return TRUE;
}

/*****************************************************************************/

static void
port_connected (MMCommandSerialPort *self,
                GParamSpec *pspec,
                gpointer user_data)
{
    gboolean connected;
    gint fd;

    if (!self->priv->channel)
        return;

    fd = g_io_channel_unix_get_fd (self->priv->channel);

    /* When the port is connected, drop the serial port lock so PPP can do
     * something with the port.  When the port is disconnected, grab the lock
     * again.
     */
    connected = mm_port_get_connected (MM_PORT (self));

    if (ioctl (fd, (connected ? TIOCNXCL : TIOCEXCL)) < 0) {
        mm_warn ("(%s): could not %s serial port lock: (%d) %s",
                 mm_port_get_device (MM_PORT (self)),
                 connected ? "drop" : "re-acquire",
                 errno,
                 strerror (errno));
        if (!connected) {
            // FIXME: do something here, maybe try again in a few seconds or
            // close the port and error out?
        }
    }

    /* When connected ignore, let PPP have all the data */
    channel_enable (self, !connected);
}

/*****************************************************************************/

static void
channel_enable (MMCommandSerialPort *self, gboolean enable)
{
    if (enable) {
        g_assert (self->priv->fd >= 0);

        g_warn_if_fail (self->priv->channel == NULL);
        self->priv->channel = g_io_channel_unix_new (self->priv->fd);
        g_io_channel_set_encoding (self->priv->channel, NULL, NULL);

        g_warn_if_fail (self->priv->watch_id == 0);
        self->priv->watch_id = g_io_add_watch (self->priv->channel,
                                               G_IO_IN | G_IO_ERR | G_IO_HUP,
                                               data_available, self);
        return;
    }

    /* Channel disable can be called multiple times */
    if (self->priv->watch_id != 0) {
        g_source_remove (self->priv->watch_id);
        self->priv->watch_id = 0;
    }

    if (self->priv->channel != NULL) {
        /* Don't shutdown: we control the fd close() in MMSerialPort */
        g_io_channel_unref (self->priv->channel);
        self->priv->channel = NULL;
    }

    /* Don't reset the fd here! */
}

static void
data_watch_enable (MMSerialPort *_self, gint fd)
{
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (_self);
    gint i;

    /* Enabling... */
    if (fd >= 0) {
        self->priv->fd = fd;
        g_warn_if_fail (self->priv->connected_id == 0);
        self->priv->connected_id = g_signal_connect (self,
                                               "notify::" MM_PORT_CONNECTED,
                                               G_CALLBACK (port_connected),
                                               NULL);
        channel_enable (self, fd >= 0);
        return;
    }

    /* Disabling... */

    if (self->priv->connected_id) {
        g_signal_handler_disconnect (self, self->priv->connected_id);
        self->priv->connected_id = 0;
    }

    /* Clear the command queue */
    for (i = 0; i < g_queue_get_length (self->priv->queue); i++) {
        MMQueueData *item = g_queue_peek_nth (self->priv->queue, i);

        if (item->callback) {
            GError *error;
            GByteArray *response;

            g_warn_if_fail (MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->handle_response != NULL);
            error = g_error_new_literal (MM_SERIAL_ERROR,
                                         MM_SERIAL_ERROR_SEND_FAILED,
                                         "Serial port is now closed");
            response = g_byte_array_sized_new (1);
            g_byte_array_append (response, (const guint8 *) "\0", 1);

            MM_COMMAND_SERIAL_PORT_GET_CLASS (self)->handle_response (self,
                                                                      response,
                                                                      error,
                                                                      item->callback,
                                                                      item->user_data);
            g_error_free (error);
            g_byte_array_free (response, TRUE);
        }

        g_clear_object (&item->cancellable);
        g_byte_array_free (item->command, TRUE);
        g_slice_free (MMQueueData, item);
    }
    g_queue_clear (self->priv->queue);

    if (self->priv->timeout_id) {
        g_source_remove (self->priv->timeout_id);
        self->priv->timeout_id = 0;
    }

    if (self->priv->queue_id) {
        g_source_remove (self->priv->queue_id);
        self->priv->queue_id = 0;
    }

    g_clear_object (&self->priv->cancellable);

    self->priv->fd = -1;
}

/*****************************************************************************/

static void
internal_queue_command (MMCommandSerialPort *self,
                        GByteArray *command,
                        gboolean take_command,
                        gboolean cached,
                        guint32 timeout_seconds,
                        GCancellable *cancellable,
                        MMCommandSerialResponseFn callback,
                        gpointer user_data)
{
    MMQueueData *info;

    g_return_if_fail (MM_IS_COMMAND_SERIAL_PORT (self));
    g_return_if_fail (command != NULL);

    if (!mm_serial_port_is_open (MM_SERIAL_PORT (self))) {
        GError *error = g_error_new_literal (MM_SERIAL_ERROR,
                                             MM_SERIAL_ERROR_SEND_FAILED,
                                             "Sending command failed: device is not enabled");
        if (callback)
            callback (self, NULL, error, user_data);
        g_error_free (error);
        return;
    }

    info = g_slice_new0 (MMQueueData);
    if (take_command)
        info->command = command;
    else {
        info->command = g_byte_array_sized_new (command->len);
        g_byte_array_append (info->command, command->data, command->len);
    }

    /* Only accept about 3 seconds of EAGAIN for this command */
    if (self->priv->send_delay)
        info->eagain_count = 3000000 / self->priv->send_delay;
    else
        info->eagain_count = 1000;

    info->cached = cached;
    info->timeout = timeout_seconds;
    info->cancellable = (cancellable ? g_object_ref (cancellable) : NULL);
    info->callback = (GCallback) callback;
    info->user_data = user_data;

    /* Clear the cached value for this command if not asking for cached value */
    if (!cached)
        set_cached_reply (self, info->command, NULL);

    g_queue_push_tail (self->priv->queue, info);

    if (g_queue_get_length (self->priv->queue) == 1)
        schedule_queue_process (self, 0);
}

void
mm_command_serial_port_queue_command (MMCommandSerialPort *self,
                                      GByteArray *command,
                                      gboolean take_command,
                                      guint32 timeout_seconds,
                                      GCancellable *cancellable,
                                      MMCommandSerialResponseFn callback,
                                      gpointer user_data)
{
    internal_queue_command (self, command, take_command, FALSE, timeout_seconds, cancellable, callback, user_data);
}

void
mm_command_serial_port_queue_command_cached (MMCommandSerialPort *self,
                                             GByteArray *command,
                                             gboolean take_command,
                                             guint32 timeout_seconds,
                                             GCancellable *cancellable,
                                             MMCommandSerialResponseFn callback,
                                             gpointer user_data)
{
    internal_queue_command (self, command, take_command, TRUE, timeout_seconds, cancellable, callback, user_data);
}

/*****************************************************************************/
/* Helpers for the cache */

static gboolean
ba_equal (gconstpointer v1, gconstpointer v2)
{
    const GByteArray *a = v1;
    const GByteArray *b = v2;

    if (!a && b)
        return -1;
    else if (a && !b)
        return 1;
    else if (!a && !b)
        return 0;

    g_assert (a && b);
    if (a->len < b->len)
        return -1;
    else if (a->len > b->len)
        return 1;

    g_assert (a->len == b->len);
    return !memcmp (a->data, b->data, a->len);
}

static guint
ba_hash (gconstpointer v)
{
    /* 31 bit hash function */
    const GByteArray *array = v;
    guint32 i, h = (const signed char) array->data[0];

    for (i = 1; i < array->len; i++)
        h = (h << 5) - h + (const signed char) array->data[i];

    return h;
}

static void
ba_free (gpointer v)
{
    g_byte_array_free ((GByteArray *) v, TRUE);
}

/*****************************************************************************/

static void
mm_command_serial_port_init (MMCommandSerialPort *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MM_TYPE_COMMAND_SERIAL_PORT, MMCommandSerialPortPrivate);

    self->priv->queue = g_queue_new ();
    self->priv->response = g_byte_array_sized_new (500);
    self->priv->reply_cache = g_hash_table_new_full (ba_hash, ba_equal, ba_free, ba_free);

    self->priv->fd = -1;
    self->priv->send_delay = 1000;
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (object);

    switch (prop_id) {
    case PROP_SEND_DELAY:
        self->priv->send_delay = g_value_get_uint64 (value);
        break;
    case PROP_SPEW_CONTROL:
        self->priv->spew_control = g_value_get_boolean (value);
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
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (object);

    switch (prop_id) {
    case PROP_SEND_DELAY:
        g_value_set_uint64 (value, self->priv->send_delay);
        break;
    case PROP_SPEW_CONTROL:
        g_value_set_boolean (value, self->priv->spew_control);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (object);

    channel_enable (self, FALSE);

    if (self->priv->connected_id) {
        g_signal_handler_disconnect (self, self->priv->connected_id);
        self->priv->connected_id = 0;
    }

    G_OBJECT_CLASS (mm_command_serial_port_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    MMCommandSerialPort *self = MM_COMMAND_SERIAL_PORT (object);

    g_hash_table_destroy (self->priv->reply_cache);
    g_byte_array_free (self->priv->response, TRUE);
    g_queue_free (self->priv->queue);

    G_OBJECT_CLASS (mm_command_serial_port_parent_class)->finalize (object);
}

static void
mm_command_serial_port_class_init (MMCommandSerialPortClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMSerialPortClass *serial_port_class = MM_SERIAL_PORT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMCommandSerialPortPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;
    object_class->finalize = finalize;
    serial_port_class->data_watch_enable = data_watch_enable;

    klass->handle_response = real_handle_response;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_SEND_DELAY,
         g_param_spec_uint64 (MM_COMMAND_SERIAL_PORT_SEND_DELAY,
                              "SendDelay",
                              "Send delay for each byte in microseconds",
                              0, G_MAXUINT64, 0,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_SPEW_CONTROL,
         g_param_spec_boolean (MM_COMMAND_SERIAL_PORT_SPEW_CONTROL,
                               "SpewControl",
                               "Spew control",
                               FALSE,
                               G_PARAM_READWRITE));

    /* Signals */
    signals[SIGNAL_BUFFER_FULL] =
        g_signal_new ("buffer-full",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MMCommandSerialPortClass, buffer_full),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_TIMED_OUT] =
        g_signal_new ("timed-out",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMCommandSerialPortClass, timed_out),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__UINT,
					  G_TYPE_NONE, 1, G_TYPE_UINT);
}
