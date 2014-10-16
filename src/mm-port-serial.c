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

#define _GNU_SOURCE  /* for strcasestr() */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <linux/serial.h>

#include <gio/gunixsocketaddress.h>

#include <ModemManager.h>
#include <mm-errors-types.h>

#include "mm-port-serial.h"
#include "mm-log.h"

static gboolean port_serial_queue_process          (gpointer data);
static void     port_serial_schedule_queue_process (MMPortSerial *self,
                                                    guint timeout_ms);
static void     port_serial_close_force            (MMPortSerial *self);
static void     port_serial_reopen_cancel          (MMPortSerial *self);
static void     port_serial_set_cached_reply       (MMPortSerial *self,
                                                    const GByteArray *command,
                                                    const GByteArray *response);

G_DEFINE_TYPE (MMPortSerial, mm_port_serial, MM_TYPE_PORT)

enum {
    PROP_0,
    PROP_BAUD,
    PROP_BITS,
    PROP_PARITY,
    PROP_STOPBITS,
    PROP_SEND_DELAY,
    PROP_FD,
    PROP_SPEW_CONTROL,
    PROP_NUL_CONTROL,
    PROP_RTS_CTS,
    PROP_FLASH_OK,

    LAST_PROP
};

enum {
    BUFFER_FULL,
    TIMED_OUT,
    FORCED_CLOSE,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define SERIAL_BUF_SIZE 2048

struct _MMPortSerialPrivate {
    guint32 open_count;
    gboolean forced_close;
    int fd;
    GHashTable *reply_cache;
    GQueue *queue;
    GByteArray *response;

    /* For real ports, iochannel, and we implement the eagain limit */
    GIOChannel *iochannel;
    guint iochannel_id;

    /* For unix-socket based ports, socket */
    GSocket *socket;
    GSource *socket_source;

    struct termios old_t;

    guint baud;
    guint bits;
    char parity;
    guint stopbits;
    guint64 send_delay;
    gboolean spew_control;
    gboolean nul_control;
    gboolean rts_cts;
    gboolean flash_ok;

    guint queue_id;
    guint timeout_id;

    GCancellable *cancellable;
    gulong cancellable_id;

    guint n_consecutive_timeouts;

    guint connected_id;

    gpointer flash_ctx;
    gpointer reopen_ctx;
};

/*****************************************************************************/
/* Command */

typedef struct {
    MMPortSerial *self;
    GSimpleAsyncResult *result;
    GCancellable *cancellable;
    GByteArray *command;
    guint32 timeout;
    gboolean allow_cached;
    guint32 eagain_count;

    guint32 idx;
    gboolean started;
    gboolean done;
} CommandContext;

static void
command_context_complete_and_free (CommandContext *ctx, gboolean idle)
{
    if (idle)
        g_simple_async_result_complete_in_idle (ctx->result);
    else
        g_simple_async_result_complete (ctx->result);
    g_object_unref (ctx->result);
    g_byte_array_unref (ctx->command);
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    g_object_unref (ctx->self);
    g_slice_free (CommandContext, ctx);
}

GByteArray *
mm_port_serial_command_finish (MMPortSerial *self,
                               GAsyncResult *res,
                               GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return g_byte_array_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

void
mm_port_serial_command (MMPortSerial *self,
                        GByteArray *command,
                        guint32 timeout_seconds,
                        gboolean allow_cached,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    CommandContext *ctx;

    g_return_if_fail (MM_IS_PORT_SERIAL (self));
    g_return_if_fail (command != NULL);

    /* Setup command context */
    ctx = g_slice_new0 (CommandContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             mm_port_serial_command);
    ctx->command = g_byte_array_ref (command);
    ctx->allow_cached = allow_cached;
    ctx->timeout = timeout_seconds;
    ctx->cancellable = (cancellable ? g_object_ref (cancellable) : NULL);

    /* Only accept about 3 seconds of EAGAIN for this command */
    if (self->priv->send_delay && mm_port_get_subsys (MM_PORT (self)) == MM_PORT_SUBSYS_TTY)
        ctx->eagain_count = 3000000 / self->priv->send_delay;
    else
        ctx->eagain_count = 1000;

    if (self->priv->open_count == 0) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_SERIAL_ERROR,
                                         MM_SERIAL_ERROR_SEND_FAILED,
                                         "Sending command failed: device is not open");
        command_context_complete_and_free (ctx, TRUE);
        return;
    }

    /* Clear the cached value for this command if not asking for cached value */
    if (!allow_cached)
        port_serial_set_cached_reply (self, ctx->command, NULL);

    g_queue_push_tail (self->priv->queue, ctx);

    if (g_queue_get_length (self->priv->queue) == 1)
        port_serial_schedule_queue_process (self, 0);
}

/*****************************************************************************/

#if 0
static const char *
baud_to_string (int baud)
{
    const char *speed = NULL;

    switch (baud) {
    case B0:
        speed = "0";
        break;
    case B50:
        speed = "50";
        break;
    case B75:
        speed = "75";
        break;
    case B110:
        speed = "110";
        break;
    case B150:
        speed = "150";
        break;
    case B300:
        speed = "300";
        break;
    case B600:
        speed = "600";
        break;
    case B1200:
        speed = "1200";
        break;
    case B2400:
        speed = "2400";
        break;
    case B4800:
        speed = "4800";
        break;
    case B9600:
        speed = "9600";
        break;
    case B19200:
        speed = "19200";
        break;
    case B38400:
        speed = "38400";
        break;
    case B57600:
        speed = "57600";
        break;
    case B115200:
        speed = "115200";
        break;
    case B460800:
        speed = "460800";
        break;
    default:
        break;
    }

    return speed;
}

void
mm_port_serial_print_config (MMPortSerial *port,
                             const char *detail)
{
    struct termios stbuf;
    int err;

    err = tcgetattr (self->priv->fd, &stbuf);
    if (err) {
        mm_warn ("*** %s (%s): (%s) tcgetattr() error %d",
                 __func__, detail, mm_port_get_device (MM_PORT (port)), errno);
        return;
    }

    mm_info ("(%s): (%s) baud rate: %d (%s)",
             detail, mm_port_get_device (MM_PORT (port)),
             stbuf.c_cflag & CBAUD,
             baud_to_string (stbuf.c_cflag & CBAUD));
}
#endif

static int
parse_baudrate (guint i)
{
    int speed;

    switch (i) {
    case 0:
        speed = B0;
        break;
    case 50:
        speed = B50;
        break;
    case 75:
        speed = B75;
        break;
    case 110:
        speed = B110;
        break;
    case 150:
        speed = B150;
        break;
    case 300:
        speed = B300;
        break;
    case 600:
        speed = B600;
        break;
    case 1200:
        speed = B1200;
        break;
    case 2400:
        speed = B2400;
        break;
    case 4800:
        speed = B4800;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    case 460800:
        speed = B460800;
        break;
    default:
        mm_warn ("Invalid baudrate '%d'", i);
        speed = B9600;
    }

    return speed;
}

static int
parse_bits (guint i)
{
    int bits;

    switch (i) {
    case 5:
        bits = CS5;
        break;
    case 6:
        bits = CS6;
        break;
    case 7:
        bits = CS7;
        break;
    case 8:
        bits = CS8;
        break;
    default:
        mm_warn ("Invalid bits (%d). Valid values are 5, 6, 7, 8.", i);
        bits = CS8;
    }

    return bits;
}

static int
parse_parity (char c)
{
    int parity;

    switch (c) {
    case 'n':
    case 'N':
        parity = 0;
        break;
    case 'e':
    case 'E':
        parity = PARENB;
        break;
    case 'o':
    case 'O':
        parity = PARENB | PARODD;
        break;
    default:
        mm_warn ("Invalid parity (%c). Valid values are n, e, o", c);
        parity = 0;
    }

    return parity;
}

static int
parse_stopbits (guint i)
{
    int stopbits;

    switch (i) {
    case 1:
        stopbits = 0;
        break;
    case 2:
        stopbits = CSTOPB;
        break;
    default:
        mm_warn ("Invalid stop bits (%d). Valid values are 1 and 2)", i);
        stopbits = 0;
    }

    return stopbits;
}

static gboolean
real_config_fd (MMPortSerial *self, int fd, GError **error)
{
    struct termios stbuf, other;
    int speed;
    int bits;
    int parity;
    int stopbits;

    /* No setup if not a tty */
    if (mm_port_get_subsys (MM_PORT (self)) != MM_PORT_SUBSYS_TTY)
        return TRUE;

    speed = parse_baudrate (self->priv->baud);
    bits = parse_bits (self->priv->bits);
    parity = parse_parity (self->priv->parity);
    stopbits = parse_stopbits (self->priv->stopbits);

    memset (&stbuf, 0, sizeof (struct termios));
    if (tcgetattr (fd, &stbuf) != 0) {
        mm_warn ("(%s): tcgetattr() error: %d",
                 mm_port_get_device (MM_PORT (self)),
                 errno);
    }

    stbuf.c_iflag &= ~(IGNCR | ICRNL | IUCLC | INPCK | IXON | IXANY );
    stbuf.c_oflag &= ~(OPOST | OLCUC | OCRNL | ONLCR | ONLRET);
    stbuf.c_lflag &= ~(ICANON | XCASE | ECHO | ECHOE | ECHONL);
    stbuf.c_lflag &= ~(ECHO | ECHOE);
    stbuf.c_cc[VMIN] = 1;
    stbuf.c_cc[VTIME] = 0;
    stbuf.c_cc[VEOF] = 1;

    /* Use software handshaking and ignore parity/framing errors */
    stbuf.c_iflag |= (IXON | IXOFF | IXANY | IGNPAR);

    /* Set up port speed and serial attributes; also ignore modem control
     * lines since most drivers don't implement RTS/CTS anyway.
     */
    stbuf.c_cflag &= ~(CBAUD | CSIZE | CSTOPB | PARENB | CRTSCTS);
    stbuf.c_cflag |= (bits | CREAD | 0 | parity | stopbits | CLOCAL);

    errno = 0;
    if (cfsetispeed (&stbuf, speed) != 0) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "%s: failed to set serial port input speed; errno %d",
                     __func__, errno);
        return FALSE;
    }

    errno = 0;
    if (cfsetospeed (&stbuf, speed) != 0) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "%s: failed to set serial port output speed; errno %d",
                     __func__, errno);
        return FALSE;
    }

    if (tcsetattr (fd, TCSANOW, &stbuf) < 0) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "%s: failed to set serial port attributes; errno %d",
                     __func__, errno);
        return FALSE;
    }

    /* tcsetattr() returns 0 if any of the requested attributes could be set,
     * so we should double-check that all were set and log a warning if not.
     */
    memset (&other, 0, sizeof (struct termios));
    errno = 0;
    if (tcgetattr (fd, &other) != 0) {
        mm_warn ("(%s): tcgetattr() error: %d",
                 mm_port_get_device (MM_PORT (self)),
                 errno);
    }

    if (memcmp (&stbuf, &other, sizeof (other)) != 0) {
        mm_warn ("(%s): port attributes not fully set",
                 mm_port_get_device (MM_PORT (self)));
    }

    return TRUE;
}

static void
serial_debug (MMPortSerial *self, const char *prefix, const char *buf, gsize len)
{
    g_return_if_fail (len > 0);

    if (MM_PORT_SERIAL_GET_CLASS (self)->debug_log)
        MM_PORT_SERIAL_GET_CLASS (self)->debug_log (self, prefix, buf, len);
}

static gboolean
port_serial_process_command (MMPortSerial *self,
                             CommandContext *ctx,
                             GError **error)
{
    const gchar *p;
    gsize written;
    gssize send_len;

    if (self->priv->iochannel == NULL && self->priv->socket == NULL) {
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
    if (ctx->started == FALSE) {
        ctx->started = TRUE;
        serial_debug (self, "-->", (const char *) ctx->command->data, ctx->command->len);
    }

    if (self->priv->send_delay == 0 || mm_port_get_subsys (MM_PORT (self)) != MM_PORT_SUBSYS_TTY) {
        /* Send the whole command in one write */
        send_len = (gssize)ctx->command->len;
        p = (gchar *)ctx->command->data;
    } else {
        /* Send just one byte of the command */
        send_len = 1;
        p = (gchar *)&ctx->command->data[ctx->idx];
    }

    /* GIOChannel based setup */
    if (self->priv->iochannel) {
        GIOStatus write_status;

        /* Send N bytes of the command */
        write_status = g_io_channel_write_chars (self->priv->iochannel, p, send_len, &written, error);
        switch (write_status) {
        case G_IO_STATUS_ERROR:
            g_prefix_error (error, "Sending command failed: ");
            return FALSE;

        case G_IO_STATUS_EOF:
            /* We shouldn't get EOF when writing */
            g_assert_not_reached ();
            break;

        case G_IO_STATUS_NORMAL:
            if (written > 0) {
                ctx->idx += written;
                break;
            }
            /* If written == 0, treat as EAGAIN, so fall down */

        case G_IO_STATUS_AGAIN:
            /* We're in a non-blocking channel and therefore we're up to receive
             * EAGAIN; just retry in this case. */
            ctx->eagain_count--;
            if (ctx->eagain_count <= 0) {
                /* If we reach the limit of EAGAIN errors, treat as a timeout error. */
                self->priv->n_consecutive_timeouts++;
                g_signal_emit (self, signals[TIMED_OUT], 0, self->priv->n_consecutive_timeouts);

                g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_SEND_FAILED,
                             "Sending command failed: '%s'", strerror (errno));
                return FALSE;
            }
            break;
        }
    }
    /* Socket based setup */
    else if (self->priv->socket) {
        GError *inner_error = NULL;
        gssize bytes_sent;

        /* Send N bytes of the command */
        bytes_sent = g_socket_send (self->priv->socket, p, send_len, NULL, &inner_error);
        if (bytes_sent < 0) {
            /* Non-EWOULDBLOCK error? */
            if (!g_error_matches (inner_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
                g_propagate_error (error, inner_error);
                g_prefix_error (error, "Sending command failed: ");
                return FALSE;
            }

            /* We're in a non-blocking socket and therefore we're up to receive
             * EWOULDBLOCK; just retry in this case. */

            g_error_free (inner_error);

            ctx->eagain_count--;
            if (ctx->eagain_count <= 0) {
                /* If we reach the limit of EAGAIN errors, treat as a timeout error. */
                self->priv->n_consecutive_timeouts++;
                g_signal_emit (self, signals[TIMED_OUT], 0, self->priv->n_consecutive_timeouts);
                g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_SEND_FAILED,
                             "Sending command failed: '%s'", strerror (errno));
                return FALSE;
            }

            /* Just keep on, will retry... */
            written = 0;
        } else
          written = bytes_sent;

        ctx->idx += written;
    } else
        g_assert_not_reached ();

    if (ctx->idx >= ctx->command->len)
        ctx->done = TRUE;

    return TRUE;
}

static void
port_serial_set_cached_reply (MMPortSerial *self,
                              const GByteArray *command,
                              const GByteArray *response)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (MM_IS_PORT_SERIAL (self));
    g_return_if_fail (command != NULL);

    if (response) {
        GByteArray *cmd_copy = g_byte_array_sized_new (command->len);
        GByteArray *rsp_copy = g_byte_array_sized_new (response->len);

        g_byte_array_append (cmd_copy, command->data, command->len);
        g_byte_array_append (rsp_copy, response->data, response->len);
        g_hash_table_insert (self->priv->reply_cache, cmd_copy, rsp_copy);
    } else
        g_hash_table_remove (self->priv->reply_cache, command);
}

static const GByteArray *
port_serial_get_cached_reply (MMPortSerial *self,
                              GByteArray *command)
{
    return (const GByteArray *)g_hash_table_lookup (self->priv->reply_cache, command);
}

static void
port_serial_schedule_queue_process (MMPortSerial *self, guint timeout_ms)
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
        self->priv->queue_id = g_timeout_add (timeout_ms, port_serial_queue_process, self);
    else
        self->priv->queue_id = g_idle_add (port_serial_queue_process, self);
}

static void
port_serial_got_response (MMPortSerial *self,
                          const GError *error)
{
    CommandContext *ctx;

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

    ctx = (CommandContext *) g_queue_pop_head (self->priv->queue);
    if (ctx) {
        if (error)
            g_simple_async_result_set_from_error (ctx->result, error);
        else {
            if (ctx->allow_cached && !error)
                port_serial_set_cached_reply (self, ctx->command, self->priv->response);

            /* Upon completion, it is a task of the caller to remove from the response
             * buffer the processed data */
            g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                       g_byte_array_ref (self->priv->response),
                                                       (GDestroyNotify)g_byte_array_unref);
        }

        /* Don't complete in idle. We need the caller remove the response range which
         * was processed, and that must be done before processing any new queued command */
        command_context_complete_and_free (ctx, FALSE);
    }

    if (!g_queue_is_empty (self->priv->queue))
        port_serial_schedule_queue_process (self, 0);
}

static gboolean
port_serial_timed_out (gpointer data)
{
    MMPortSerial *self = MM_PORT_SERIAL (data);
    GError *error;

    self->priv->timeout_id = 0;

    /* Update number of consecutive timeouts found */
    self->priv->n_consecutive_timeouts++;

    /* FIXME: This is not completely correct - if the response finally arrives and there's
     * some other command waiting for response right now, the other command will
     * get the output of the timed out command. Not sure what to do here. */
    error = g_error_new_literal (MM_SERIAL_ERROR,
                                 MM_SERIAL_ERROR_RESPONSE_TIMEOUT,
                                 "Serial command timed out");
    port_serial_got_response (self, error);
    g_error_free (error);

    /* Emit a timed out signal, used by upper layers to identify a disconnected
     * serial port */
    g_signal_emit (self, signals[TIMED_OUT], 0, self->priv->n_consecutive_timeouts);

    return FALSE;
}

static void
port_serial_response_wait_cancelled (GCancellable *cancellable,
                                     MMPortSerial *self)
{
    GError *error;

    /* We don't want to call disconnect () while in the signal handler */
    self->priv->cancellable_id = 0;

    /* FIXME: This is not completely correct - if the response finally arrives and there's
     * some other command waiting for response right now, the other command will
     * get the output of the cancelled command. Not sure what to do here. */
    error = g_error_new_literal (MM_CORE_ERROR,
                                 MM_CORE_ERROR_CANCELLED,
                                 "Waiting for the reply cancelled");
    port_serial_got_response (self, error);
    g_error_free (error);
}

static gboolean
port_serial_queue_process (gpointer data)
{
    MMPortSerial *self = MM_PORT_SERIAL (data);
    CommandContext *ctx;
    GError *error = NULL;

    self->priv->queue_id = 0;

    ctx = (CommandContext *) g_queue_peek_head (self->priv->queue);
    if (!ctx)
        return FALSE;

    if (ctx->allow_cached) {
        const GByteArray *cached;

        cached = port_serial_get_cached_reply (self, ctx->command);
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
            port_serial_got_response (self, NULL);
            return FALSE;
        }

        /* Cached reply wasn't found, keep on */
    }

    /* If error, report it */
    if (!port_serial_process_command (self, ctx, &error)) {
        port_serial_got_response (self, error);
        g_error_free (error);
        return FALSE;
    }

    /* Schedule the next byte of the command to be sent */
    if (!ctx->done) {
        port_serial_schedule_queue_process (self,
                                            (mm_port_get_subsys (MM_PORT (self)) == MM_PORT_SUBSYS_TTY ?
                                             self->priv->send_delay / 1000 :
                                             0));
        return FALSE;
    }

    /* Setup the cancellable so that we can stop waiting for a response */
    if (ctx->cancellable) {
        self->priv->cancellable = g_object_ref (ctx->cancellable);
        self->priv->cancellable_id = (g_cancellable_connect (
                                          ctx->cancellable,
                                          (GCallback)port_serial_response_wait_cancelled,
                                          self,
                                          NULL));
        if (!self->priv->cancellable_id) {
            error = g_error_new (MM_CORE_ERROR,
                                 MM_CORE_ERROR_CANCELLED,
                                 "Won't wait for the reply");
            port_serial_got_response (self, error);
            g_error_free (error);
            return FALSE;
        }
    }

    /* If the command is finished being sent, schedule the timeout */
    self->priv->timeout_id = g_timeout_add_seconds (ctx->timeout,
                                                    port_serial_timed_out,
                                                    self);
    return FALSE;
}

static gboolean
parse_response (MMPortSerial *self,
                GByteArray *response,
                GError **error)
{
    if (MM_PORT_SERIAL_GET_CLASS (self)->parse_unsolicited)
        MM_PORT_SERIAL_GET_CLASS (self)->parse_unsolicited (self, response);

    g_return_val_if_fail (MM_PORT_SERIAL_GET_CLASS (self)->parse_response, FALSE);
    return MM_PORT_SERIAL_GET_CLASS (self)->parse_response (self, response, error);
}

static gboolean
common_input_available (MMPortSerial *self,
                        GIOCondition condition)
{
    char buf[SERIAL_BUF_SIZE + 1];
    gsize bytes_read;
    GIOStatus status = G_IO_STATUS_NORMAL;
    CommandContext *ctx;
    const char *device;
    GError *error = NULL;

    if (condition & G_IO_HUP) {
        device = mm_port_get_device (MM_PORT (self));
        mm_dbg ("(%s) unexpected port hangup!", device);

        if (self->priv->response->len)
            g_byte_array_remove_range (self->priv->response, 0, self->priv->response->len);
        port_serial_close_force (self);
        return FALSE;
    }

    if (condition & G_IO_ERR) {
        if (self->priv->response->len)
            g_byte_array_remove_range (self->priv->response, 0, self->priv->response->len);
        return TRUE;
    }

    /* Don't read any input if the current command isn't done being sent yet */
    ctx = g_queue_peek_nth (self->priv->queue, 0);
    if (ctx && (ctx->started == TRUE) && (ctx->done == FALSE))
        return TRUE;

    do {
        bytes_read = 0;

        if (self->priv->iochannel) {
            status = g_io_channel_read_chars (self->priv->iochannel,
                                              buf,
                                              SERIAL_BUF_SIZE,
                                              &bytes_read,
                                              &error);
            if (status == G_IO_STATUS_ERROR) {
                if (error) {
                    mm_warn ("(%s): read error: %s",
                             mm_port_get_device (MM_PORT (self)),
                             error->message);
                }
                g_clear_error (&error);
            }
        } else if (self->priv->socket) {
            bytes_read = g_socket_receive (self->priv->socket,
                                           buf,
                                           SERIAL_BUF_SIZE,
                                           NULL, /* cancellable */
                                           &error);
            if (bytes_read == -1) {
                bytes_read = 0;
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
                    status = G_IO_STATUS_AGAIN;
                else
                    status = G_IO_STATUS_ERROR;
                mm_warn ("(%s): receive error: %s",
                         mm_port_get_device (MM_PORT (self)),
                         error->message);
                g_clear_error (&error);
            } else
                status = G_IO_STATUS_NORMAL;
        }

        /* If no bytes read, just wait for more data */
        if (bytes_read == 0)
            break;

        /* Remove all NUL bytes from the received data */
        if (self->priv->nul_control) {
            gchar newbuf[SERIAL_BUF_SIZE + 1];
            guint i, j;

            /* Remove all NUL bytes found in the stream */
            for (i = 0, j = 0; i < bytes_read; i++) {
                if (buf[i] != '\0')
                    newbuf[j++] = buf[i];
            }

            g_debug ("(%s) nul control: removed %u bytes from input data, appended %u bytes",
                     mm_port_get_device (MM_PORT (self)),
                     (guint) (bytes_read - j),
                     j);

            if (j == 0)
                break;

            serial_debug (self, "<--", newbuf, j);
            g_byte_array_append (self->priv->response, (const guint8 *) newbuf, j);
        } else {
            serial_debug (self, "<--", buf, bytes_read);
            g_byte_array_append (self->priv->response, (const guint8 *) buf, bytes_read);
        }

        /* Make sure the response doesn't grow too long */
        if ((self->priv->response->len > SERIAL_BUF_SIZE) && self->priv->spew_control) {
            /* Notify listeners and then trim the buffer */
            g_signal_emit (self, signals[BUFFER_FULL], 0, self->priv->response);
            g_byte_array_remove_range (self->priv->response, 0, (SERIAL_BUF_SIZE / 2));
        }

        /* Parse response. Returns TRUE either if an error is provided or if
         * we really have the response to process. */
        if (parse_response (self, self->priv->response, &error)) {
            /* Reset number of consecutive timeouts only here */
            self->priv->n_consecutive_timeouts = 0;
            /* Process response retrieved */
            port_serial_got_response (self, error);
            g_clear_error (&error);
        }
    } while (   (bytes_read == SERIAL_BUF_SIZE || status == G_IO_STATUS_AGAIN)
             && (self->priv->iochannel_id > 0 || self->priv->socket_source != NULL));

    return TRUE;
}

static gboolean
iochannel_input_available (GIOChannel *iochannel,
                           GIOCondition condition,
                           gpointer data)
{
    return common_input_available (MM_PORT_SERIAL (data), condition);
}

static gboolean
socket_input_available (GSocket *socket,
                        GIOCondition condition,
                        gpointer data)
{
    return common_input_available (MM_PORT_SERIAL (data), condition);
}

static void
data_watch_enable (MMPortSerial *self, gboolean enable)
{
    if (self->priv->iochannel_id) {
        if (enable)
            g_warn_if_fail (self->priv->iochannel_id == 0);

        g_source_remove (self->priv->iochannel_id);
        self->priv->iochannel_id = 0;
    }

    if (self->priv->socket_source) {
        if (enable)
            g_warn_if_fail (self->priv->socket_source == NULL);
        g_source_destroy (self->priv->socket_source);
        g_source_unref (self->priv->socket_source);
        self->priv->socket_source = NULL;
    }

    if (enable) {
        if (self->priv->iochannel) {
            self->priv->iochannel_id = g_io_add_watch (self->priv->iochannel,
                                                       G_IO_IN | G_IO_ERR | G_IO_HUP,
                                                       iochannel_input_available,
                                                       self);
        } else if (self->priv->socket) {
            self->priv->socket_source = g_socket_create_source (self->priv->socket,
                                                                G_IO_IN | G_IO_ERR | G_IO_HUP,
                                                                NULL);
            g_source_set_callback (self->priv->socket_source,
                                   (GSourceFunc)socket_input_available,
                                   self,
                                   NULL);
            g_source_attach (self->priv->socket_source, NULL);
        }
        else
            g_warn_if_reached ();
    }
}

static void
port_connected (MMPortSerial *self, GParamSpec *pspec, gpointer user_data)
{
    gboolean connected;

    if (!self->priv->iochannel && !self->priv->socket)
        return;

    /* When the port is connected, drop the serial port lock so PPP can do
     * something with the port.  When the port is disconnected, grab the lock
     * again.
     */
    connected = mm_port_get_connected (MM_PORT (self));

    if (self->priv->fd >= 0 && ioctl (self->priv->fd, (connected ? TIOCNXCL : TIOCEXCL)) < 0) {
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

    /* When connected ignore let PPP have all the data */
    data_watch_enable (self, !connected);
}

gboolean
mm_port_serial_open (MMPortSerial *self, GError **error)
{
    char *devfile;
    const char *device;
    struct serial_struct sinfo = { 0 };
    GTimeVal tv_start, tv_end;
    int errno_save = 0;

    g_return_val_if_fail (MM_IS_PORT_SERIAL (self), FALSE);

    device = mm_port_get_device (MM_PORT (self));

    if (self->priv->forced_close) {
        g_set_error (error,
                     MM_SERIAL_ERROR,
                     MM_SERIAL_ERROR_OPEN_FAILED,
                     "Could not open serial device %s: it has been forced close",
                     device);
        return FALSE;
    }

    if (self->priv->reopen_ctx) {
        g_set_error (error,
                     MM_SERIAL_ERROR,
                     MM_SERIAL_ERROR_OPEN_FAILED,
                     "Could not open serial device %s: reopen operation in progress",
                     device);
        return FALSE;
    }

    if (self->priv->open_count) {
        /* Already open */
        goto success;
    }

    mm_dbg ("(%s) opening serial port...", device);

    g_get_current_time (&tv_start);

    /* Non-socket setup needs the fd open */
    if (mm_port_get_subsys (MM_PORT (self)) != MM_PORT_SUBSYS_UNIX) {
        /* Only open a new file descriptor if we weren't given one already */
        if (self->priv->fd < 0) {
            devfile = g_strdup_printf ("/dev/%s", device);
            errno = 0;
            self->priv->fd = open (devfile, O_RDWR | O_EXCL | O_NONBLOCK | O_NOCTTY);
            errno_save = errno;
            g_free (devfile);
        }

        if (self->priv->fd < 0) {
            /* nozomi isn't ready yet when the port appears, and it'll return
             * ENODEV when open(2) is called on it.  Make sure we can handle this
             * by returning a special error in that case.
             */
            g_set_error (error,
                         MM_SERIAL_ERROR,
                         (errno == ENODEV) ? MM_SERIAL_ERROR_OPEN_FAILED_NO_DEVICE : MM_SERIAL_ERROR_OPEN_FAILED,
                         "Could not open serial device %s: %s", device, strerror (errno_save));
            mm_warn ("(%s) could not open serial device (%d)", device, errno_save);
            return FALSE;
        }
    }

    /* Serial port specific setup */
    if (mm_port_get_subsys (MM_PORT (self)) == MM_PORT_SUBSYS_TTY) {
        /* Try to lock serial device */
        if (ioctl (self->priv->fd, TIOCEXCL) < 0) {
            errno_save = errno;
            g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_OPEN_FAILED,
                         "Could not lock serial device %s: %s", device, strerror (errno_save));
            mm_warn ("(%s) could not lock serial device (%d)", device, errno_save);
            goto error;
        }

        /* Flush any waiting IO */
        tcflush (self->priv->fd, TCIOFLUSH);

        if (tcgetattr (self->priv->fd, &self->priv->old_t) < 0) {
            errno_save = errno;
            g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_OPEN_FAILED,
                         "Could not set attributes on serial device %s: %s", device, strerror (errno_save));
            mm_warn ("(%s) could not set attributes on serial device (%d)", device, errno_save);
            goto error;
        }

        /* Don't wait for pending data when closing the port; this can cause some
         * stupid devices that don't respond to URBs on a particular port to hang
         * for 30 seconds when probing fails.  See GNOME bug #630670.
         */
        if (ioctl (self->priv->fd, TIOCGSERIAL, &sinfo) == 0) {
            sinfo.closing_wait = ASYNC_CLOSING_WAIT_NONE;
            if (ioctl (self->priv->fd, TIOCSSERIAL, &sinfo) < 0)
                mm_warn ("(%s): couldn't set serial port closing_wait to none: %s",
                         device, g_strerror (errno));
        }
    }

    g_warn_if_fail (MM_PORT_SERIAL_GET_CLASS (self)->config_fd);
    if (self->priv->fd >= 0 && !MM_PORT_SERIAL_GET_CLASS (self)->config_fd (self, self->priv->fd, error)) {
        mm_dbg ("(%s) failed to configure serial device", device);
        goto error;
    }

    g_get_current_time (&tv_end);

    if (tv_end.tv_sec - tv_start.tv_sec > 7)
        mm_warn ("(%s): open blocked by driver for more than 7 seconds!", device);

    if (mm_port_get_subsys (MM_PORT (self)) != MM_PORT_SUBSYS_UNIX) {
        /* Create new GIOChannel */
        self->priv->iochannel = g_io_channel_unix_new (self->priv->fd);

        /* We don't want UTF-8 encoding, we're playing with raw binary data */
        g_io_channel_set_encoding (self->priv->iochannel, NULL, NULL);

        /* We don't want to get the channel buffered */
        g_io_channel_set_buffered (self->priv->iochannel, FALSE);

        /* We don't want to get blocked while writing stuff */
        if (!g_io_channel_set_flags (self->priv->iochannel,
                                     G_IO_FLAG_NONBLOCK,
                                     error)) {
            g_prefix_error (error, "Cannot set non-blocking channel: ");
            goto error;
        }
    } else {
        GSocketAddress *address;

        /* Create new GSocket */
        self->priv->socket = g_socket_new (G_SOCKET_FAMILY_UNIX,
                                           G_SOCKET_TYPE_STREAM,
                                           G_SOCKET_PROTOCOL_DEFAULT,
                                           error);
        if (!self->priv->socket) {
            g_prefix_error (error, "Cannot create socket: ");
            goto error;
        }

        /* Non-blocking socket */
        g_socket_set_blocking (self->priv->socket, FALSE);

        /* By default, abstract socket */
        address = (g_unix_socket_address_new_with_type (
                       device,
                       -1,
                       (g_str_has_prefix (device, "abstract:") ?
                        G_UNIX_SOCKET_ADDRESS_ABSTRACT :
                        G_UNIX_SOCKET_ADDRESS_PATH)));

        /* Connect to address */
        if (!g_socket_connect (self->priv->socket,
                               address,
                               NULL,
                               error)) {
            g_prefix_error (error, "Cannot connect socket: ");
            g_object_unref (address);
            goto error;
        }
        g_object_unref (address);
    }

    /* Reading watch enable */
    data_watch_enable (self, TRUE);

    g_warn_if_fail (self->priv->connected_id == 0);
    self->priv->connected_id = g_signal_connect (self,
                                                 "notify::" MM_PORT_CONNECTED,
                                                 G_CALLBACK (port_connected),
                                                 NULL);

success:
    self->priv->open_count++;
    mm_dbg ("(%s) device open count is %d (open)", device, self->priv->open_count);

    /* Run additional port config if just opened */
    if (self->priv->open_count == 1 && MM_PORT_SERIAL_GET_CLASS (self)->config)
        MM_PORT_SERIAL_GET_CLASS (self)->config (self);

    return TRUE;

error:
    mm_warn ("(%s) failed to open serial device", device);

    if (self->priv->iochannel) {
        g_io_channel_shutdown (self->priv->iochannel, FALSE, NULL);
        g_io_channel_unref (self->priv->iochannel);
        self->priv->iochannel = NULL;
    }

    if (self->priv->socket) {
        g_socket_close (self->priv->socket, NULL);
        g_object_unref (self->priv->socket);
        self->priv->socket = NULL;
    }

    if (self->priv->fd >= 0) {
        close (self->priv->fd);
        self->priv->fd = -1;
    }

    return FALSE;
}

gboolean
mm_port_serial_is_open (MMPortSerial *self)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (MM_IS_PORT_SERIAL (self), FALSE);

    return !!self->priv->open_count;
}

static void
_close_internal (MMPortSerial *self, gboolean force)
{
    const char *device;
    int i;

    g_return_if_fail (MM_IS_PORT_SERIAL (self));

    if (force)
        self->priv->open_count = 0;
    else {
        g_return_if_fail (self->priv->open_count > 0);
        self->priv->open_count--;
    }

    device = mm_port_get_device (MM_PORT (self));

    mm_dbg ("(%s) device open count is %d (close)", device, self->priv->open_count);

    if (self->priv->open_count > 0)
        return;

    if (self->priv->connected_id) {
        g_signal_handler_disconnect (self, self->priv->connected_id);
        self->priv->connected_id = 0;
    }

    mm_port_serial_flash_cancel (self);

    if (self->priv->iochannel || self->priv->socket) {
        GTimeVal tv_start, tv_end;
        struct serial_struct sinfo = { 0 };

        mm_dbg ("(%s) closing serial port...", device);

        mm_port_set_connected (MM_PORT (self), FALSE);

        g_get_current_time (&tv_start);

        /* Serial port specific setup */
        if (self->priv->fd >= 0 && mm_port_get_subsys (MM_PORT (self)) == MM_PORT_SUBSYS_TTY) {
            /* Paranoid: ensure our closing_wait value is still set so we ignore
             * pending data when closing the port.  See GNOME bug #630670.
             */
            if (ioctl (self->priv->fd, TIOCGSERIAL, &sinfo) == 0) {
                if (sinfo.closing_wait != ASYNC_CLOSING_WAIT_NONE) {
                    mm_warn ("(%s): serial port closing_wait was reset!", device);
                    sinfo.closing_wait = ASYNC_CLOSING_WAIT_NONE;
                    if (ioctl (self->priv->fd, TIOCSSERIAL, &sinfo) < 0)
                        mm_warn ("(%s): couldn't set serial port closing_wait to none: %s",
                                 device, g_strerror (errno));
                }
            }

            tcsetattr (self->priv->fd, TCSANOW, &self->priv->old_t);
            tcflush (self->priv->fd, TCIOFLUSH);
        }

        /* Destroy channel */
        if (self->priv->iochannel) {
            data_watch_enable (self, FALSE);
            g_io_channel_shutdown (self->priv->iochannel, TRUE, NULL);
            g_io_channel_unref (self->priv->iochannel);
            self->priv->iochannel = NULL;
        }

        /* Close fd, if any */
        if (self->priv->fd >= 0) {
            close (self->priv->fd);
            self->priv->fd = -1;
        }

        /* Destroy socket */
        if (self->priv->socket) {
            data_watch_enable (self, FALSE);
            g_socket_close (self->priv->socket, NULL);
            g_object_unref (self->priv->socket);
            self->priv->socket = NULL;
        }

        g_get_current_time (&tv_end);

        mm_dbg ("(%s) serial port closed", device);

        /* Some ports don't respond to data and when close is called
         * the serial layer waits up to 30 second (closing_wait) for
         * that data to send before giving up and returning from close().
         * Log that.  See GNOME bug #630670 for more details.
         */
        if (tv_end.tv_sec - tv_start.tv_sec > 7)
            mm_warn ("(%s): close blocked by driver for more than 7 seconds!", device);
    }

    /* Clear the command queue */
    for (i = 0; i < g_queue_get_length (self->priv->queue); i++) {
        CommandContext *ctx;

        ctx = g_queue_peek_nth (self->priv->queue, i);
        g_simple_async_result_set_error (ctx->result,
                                         MM_SERIAL_ERROR,
                                         MM_SERIAL_ERROR_SEND_FAILED,
                                         "Serial port is now closed");
        command_context_complete_and_free (ctx, TRUE);
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

    if (self->priv->cancellable_id) {
        g_assert (self->priv->cancellable != NULL);
        g_cancellable_disconnect (self->priv->cancellable,
                                  self->priv->cancellable_id);
        self->priv->cancellable_id = 0;
    }

    g_clear_object (&self->priv->cancellable);
}

void
mm_port_serial_close (MMPortSerial *self)
{
    g_return_if_fail (MM_IS_PORT_SERIAL (self));

    if (!self->priv->forced_close)
        _close_internal (self, FALSE);
}

static void
port_serial_close_force (MMPortSerial *self)
{
    g_return_if_fail (MM_IS_PORT_SERIAL (self));

    /* If already forced to close, return */
    if (self->priv->forced_close)
        return;

    mm_dbg ("(%s) forced to close port", mm_port_get_device (MM_PORT (self)));

    /* Mark as having forced the close, so that we don't warn about incorrect
     * open counts */
    self->priv->forced_close = TRUE;

    /* Cancel port reopening if one is running */
    port_serial_reopen_cancel (self);

    /* If already closed, done */
    if (self->priv->open_count > 0) {
        _close_internal (self, TRUE);

        /* Notify about the forced close status */
        g_signal_emit (self, signals[FORCED_CLOSE], 0);
    }
}

/*****************************************************************************/
/* Reopen */

typedef struct {
    MMPortSerial *self;
    GSimpleAsyncResult *result;
    guint initial_open_count;
    guint reopen_id;
} ReopenContext;

static void
reopen_context_complete_and_free (ReopenContext *ctx)
{
    if (ctx->reopen_id)
        g_source_remove (ctx->reopen_id);
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (ReopenContext, ctx);
}

gboolean
mm_port_serial_reopen_finish (MMPortSerial *port,
                              GAsyncResult *res,
                              GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
port_serial_reopen_cancel (MMPortSerial *self)
{
    ReopenContext *ctx;

    if (!self->priv->reopen_ctx)
        return;

    /* Recover context */
    ctx = (ReopenContext *)self->priv->reopen_ctx;
    self->priv->reopen_ctx = NULL;

    g_simple_async_result_set_error (ctx->result,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_CANCELLED,
                                     "Reopen cancelled");
    reopen_context_complete_and_free (ctx);
}

static gboolean
reopen_do (MMPortSerial *self)
{
    ReopenContext *ctx;
    GError *error = NULL;
    guint i;

    /* Recover context */
    g_assert (self->priv->reopen_ctx != NULL);
    ctx = (ReopenContext *)self->priv->reopen_ctx;
    self->priv->reopen_ctx = NULL;

    ctx->reopen_id = 0;

    for (i = 0; i < ctx->initial_open_count; i++) {
        if (!mm_port_serial_open (ctx->self, &error)) {
            g_prefix_error (&error, "Couldn't reopen port (%u): ", i);
            break;
        }
    }

    if (error)
        g_simple_async_result_take_error (ctx->result, error);
    else
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    reopen_context_complete_and_free (ctx);

    return FALSE;
}

void
mm_port_serial_reopen (MMPortSerial *self,
                       guint32 reopen_time,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
    ReopenContext *ctx;
    guint i;

    g_return_if_fail (MM_IS_PORT_SERIAL (self));

    /* Setup context */
    ctx = g_slice_new0 (ReopenContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             mm_port_serial_reopen);
    ctx->initial_open_count = self->priv->open_count;

    if (self->priv->forced_close) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "Serial port has been forced close.");
        reopen_context_complete_and_free (ctx);
        return;
    }

    /* If already reopening, halt */
    if (self->priv->reopen_ctx) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_IN_PROGRESS,
                                         "Modem is already being reopened");
        reopen_context_complete_and_free (ctx);
        return;
    }

    mm_dbg ("(%s) reopening port (%u)",
            mm_port_get_device (MM_PORT (self)),
            ctx->initial_open_count);

    for (i = 0; i < ctx->initial_open_count; i++)
        mm_port_serial_close (self);

    if (reopen_time > 0)
        ctx->reopen_id = g_timeout_add (reopen_time, (GSourceFunc)reopen_do, self);
    else
        ctx->reopen_id = g_idle_add ((GSourceFunc)reopen_do, self);

    /* Store context in private info */
    self->priv->reopen_ctx = ctx;
}

static gboolean
get_speed (MMPortSerial *self, speed_t *speed, GError **error)
{
    struct termios options;

    g_assert (self->priv->fd >= 0);

    memset (&options, 0, sizeof (struct termios));
    if (tcgetattr (self->priv->fd, &options) != 0) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "%s: tcgetattr() error %d",
                     __func__, errno);
        return FALSE;
    }

    *speed = cfgetospeed (&options);
    return TRUE;
}

static gboolean
set_speed (MMPortSerial *self, speed_t speed, GError **error)
{
    struct termios options;
    int fd, count = 4;
    gboolean success = FALSE;

    g_assert (self->priv->fd >= 0);

    fd = self->priv->fd;

    memset (&options, 0, sizeof (struct termios));
    if (tcgetattr (fd, &options) != 0) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "%s: tcgetattr() error %d",
                     __func__, errno);
        return FALSE;
    }

    cfsetispeed (&options, speed);
    cfsetospeed (&options, speed);
    options.c_cflag |= (CLOCAL | CREAD);

    /* Configure flow control as well here */
    if (self->priv->rts_cts)
        options.c_cflag |= (CRTSCTS);

    while (count-- > 0) {
        if (tcsetattr (fd, TCSANOW, &options) == 0) {
            success = TRUE;
            break;  /* Operation successful */
        }

        /* Try a few times if EAGAIN */
        if (errno == EAGAIN)
            g_usleep (100000);
        else {
            /* If not EAGAIN, hard error */
            g_set_error (error,
                            MM_CORE_ERROR,
                            MM_CORE_ERROR_FAILED,
                            "%s: tcsetattr() error %d",
                            __func__, errno);
            return FALSE;
        }
    }

    if (!success) {
        g_set_error (error,
                        MM_CORE_ERROR,
                        MM_CORE_ERROR_FAILED,
                        "%s: tcsetattr() retry timeout",
                        __func__);
        return FALSE;
    }

    return TRUE;
}

/*****************************************************************************/
/* Flash */

typedef struct {
    GSimpleAsyncResult *result;
    MMPortSerial *self;
    speed_t current_speed;
    guint flash_id;
} FlashContext;

static void
flash_context_complete_and_free (FlashContext *ctx)
{
    if (ctx->flash_id)
        g_source_remove (ctx->flash_id);
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (FlashContext, ctx);
}

gboolean
mm_port_serial_flash_finish (MMPortSerial *port,
                             GAsyncResult *res,
                             GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

void
mm_port_serial_flash_cancel (MMPortSerial *self)
{
    FlashContext *ctx;

    if (!self->priv->flash_ctx)
        return;

    /* Recover context */
    ctx = (FlashContext *)self->priv->flash_ctx;
    self->priv->flash_ctx = NULL;

    g_simple_async_result_set_error (ctx->result,
                                     MM_CORE_ERROR,
                                     MM_CORE_ERROR_CANCELLED,
                                     "Flash cancelled");
    flash_context_complete_and_free (ctx);
}

static gboolean
flash_do (MMPortSerial *self)
{
    FlashContext *ctx;
    GError *error = NULL;

    /* Recover context */
    g_assert (self->priv->flash_ctx != NULL);
    ctx = (FlashContext *)self->priv->flash_ctx;
    self->priv->flash_ctx = NULL;

    ctx->flash_id = 0;

    if (self->priv->flash_ok && mm_port_get_subsys (MM_PORT (self)) == MM_PORT_SUBSYS_TTY) {
        if (ctx->current_speed) {
            if (!set_speed (ctx->self, ctx->current_speed, &error))
                g_assert (error);
        } else {
            error = g_error_new_literal (MM_SERIAL_ERROR,
                                         MM_SERIAL_ERROR_FLASH_FAILED,
                                         "Failed to retrieve current speed");
        }
    }

    if (error)
        g_simple_async_result_take_error (ctx->result, error);
    else
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
    flash_context_complete_and_free (ctx);

    return FALSE;
}

void
mm_port_serial_flash (MMPortSerial *self,
                      guint32 flash_time,
                      gboolean ignore_errors,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
    FlashContext *ctx;
    GError *error = NULL;
    gboolean success;

    g_return_if_fail (MM_IS_PORT_SERIAL (self));

    /* Setup context */
    ctx = g_slice_new0 (FlashContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             mm_port_serial_flash);

    if (!mm_port_serial_is_open (self)) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_SERIAL_ERROR,
                                         MM_SERIAL_ERROR_NOT_OPEN,
                                         "The serial port is not open.");
        flash_context_complete_and_free (ctx);
        return;
    }

    if (self->priv->flash_ctx) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_IN_PROGRESS,
                                         "Modem is already being flashed.");
        flash_context_complete_and_free (ctx);
        return;
    }

    /* Flashing only in TTY */
    if (!self->priv->flash_ok || mm_port_get_subsys (MM_PORT (self)) != MM_PORT_SUBSYS_TTY) {
        self->priv->flash_ctx = ctx;
        ctx->flash_id = g_idle_add ((GSourceFunc)flash_do, self);
        return;
    }

    /* Grab current speed so we can reset it after flashing */
    success = get_speed (self, &ctx->current_speed, &error);
    if (!success && !ignore_errors) {
        g_simple_async_result_take_error (ctx->result, error);
        flash_context_complete_and_free (ctx);
        return;
    }
    g_clear_error (&error);

    success = set_speed (self, B0, &error);
    if (!success && !ignore_errors) {
        g_simple_async_result_take_error (ctx->result, error);
        flash_context_complete_and_free (ctx);
        return;
    }
    g_clear_error (&error);

    self->priv->flash_ctx = ctx;
    ctx->flash_id = g_timeout_add (flash_time, (GSourceFunc)flash_do, self);
}

/*****************************************************************************/

MMPortSerial *
mm_port_serial_new (const char *name, MMPortType ptype)
{
    return MM_PORT_SERIAL (g_object_new (MM_TYPE_PORT_SERIAL,
                                         MM_PORT_DEVICE, name,
                                         MM_PORT_SUBSYS, MM_PORT_SUBSYS_TTY,
                                         MM_PORT_TYPE, ptype,
                                         NULL));
}

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
    g_byte_array_unref ((GByteArray *) v);
}

static void
mm_port_serial_init (MMPortSerial *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MM_TYPE_PORT_SERIAL, MMPortSerialPrivate);

    self->priv->reply_cache = g_hash_table_new_full (ba_hash, ba_equal, ba_free, ba_free);

    self->priv->fd = -1;
    self->priv->baud = 57600;
    self->priv->bits = 8;
    self->priv->parity = 'n';
    self->priv->stopbits = 1;
    self->priv->send_delay = 1000;

    self->priv->queue = g_queue_new ();
    self->priv->response = g_byte_array_sized_new (500);
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    MMPortSerial *self = MM_PORT_SERIAL (object);

    switch (prop_id) {
    case PROP_FD:
        self->priv->fd = g_value_get_int (value);
        break;
    case PROP_BAUD:
        self->priv->baud = g_value_get_uint (value);
        break;
    case PROP_BITS:
        self->priv->bits = g_value_get_uint (value);
        break;
    case PROP_PARITY:
        self->priv->parity = g_value_get_schar (value);
        break;
    case PROP_STOPBITS:
        self->priv->stopbits = g_value_get_uint (value);
        break;
    case PROP_SEND_DELAY:
        self->priv->send_delay = g_value_get_uint64 (value);
        break;
    case PROP_SPEW_CONTROL:
        self->priv->spew_control = g_value_get_boolean (value);
        g_debug ("SPEW CONTROL UPDATED: %s", self->priv->spew_control ? "yes" : "no");
        break;
    case PROP_NUL_CONTROL:
        self->priv->nul_control = g_value_get_boolean (value);
        g_debug ("NUL CONTROL UPDATED: %s", self->priv->nul_control ? "yes" : "no");
        break;
    case PROP_RTS_CTS:
        self->priv->rts_cts = g_value_get_boolean (value);
        break;
    case PROP_FLASH_OK:
        self->priv->flash_ok = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
    MMPortSerial *self = MM_PORT_SERIAL (object);

    switch (prop_id) {
    case PROP_FD:
        g_value_set_int (value, self->priv->fd);
        break;
    case PROP_BAUD:
        g_value_set_uint (value, self->priv->baud);
        break;
    case PROP_BITS:
        g_value_set_uint (value, self->priv->bits);
        break;
    case PROP_PARITY:
        g_value_set_schar (value, self->priv->parity);
        break;
    case PROP_STOPBITS:
        g_value_set_uint (value, self->priv->stopbits);
        break;
    case PROP_SEND_DELAY:
        g_value_set_uint64 (value, self->priv->send_delay);
        break;
    case PROP_SPEW_CONTROL:
        g_value_set_boolean (value, self->priv->spew_control);
        break;
    case PROP_NUL_CONTROL:
        g_value_set_boolean (value, self->priv->nul_control);
        break;
    case PROP_RTS_CTS:
        g_value_set_boolean (value, self->priv->rts_cts);
        break;
    case PROP_FLASH_OK:
        g_value_set_boolean (value, self->priv->flash_ok);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    MMPortSerial *self = MM_PORT_SERIAL (object);

    if (self->priv->timeout_id) {
        g_source_remove (self->priv->timeout_id);
        self->priv->timeout_id = 0;
    }

    port_serial_close_force (MM_PORT_SERIAL (object));
    mm_port_serial_flash_cancel (MM_PORT_SERIAL (object));

    G_OBJECT_CLASS (mm_port_serial_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    MMPortSerial *self = MM_PORT_SERIAL (object);

    g_hash_table_destroy (self->priv->reply_cache);
    g_byte_array_unref (self->priv->response);
    g_queue_free (self->priv->queue);

    G_OBJECT_CLASS (mm_port_serial_parent_class)->finalize (object);
}

static void
mm_port_serial_class_init (MMPortSerialClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMPortSerialPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;
    object_class->finalize = finalize;

    klass->config_fd = real_config_fd;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_FD,
         g_param_spec_int (MM_PORT_SERIAL_FD,
                           "File descriptor",
                           "File descriptor",
                           -1, G_MAXINT, -1,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property
        (object_class, PROP_BAUD,
         g_param_spec_uint (MM_PORT_SERIAL_BAUD,
                            "Baud",
                            "Baud rate",
                            0, G_MAXUINT, 57600,
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_BITS,
         g_param_spec_uint (MM_PORT_SERIAL_BITS,
                            "Bits",
                            "Bits",
                            5, 8, 8,
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_PARITY,
         g_param_spec_char (MM_PORT_SERIAL_PARITY,
                            "Parity",
                            "Parity",
                            'E', 'o', 'n',
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_STOPBITS,
         g_param_spec_uint (MM_PORT_SERIAL_STOPBITS,
                            "Stopbits",
                            "Stopbits",
                            1, 2, 1,
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_SEND_DELAY,
         g_param_spec_uint64 (MM_PORT_SERIAL_SEND_DELAY,
                              "SendDelay",
                              "Send delay for each byte in microseconds",
                              0, G_MAXUINT64, 0,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_SPEW_CONTROL,
         g_param_spec_boolean (MM_PORT_SERIAL_SPEW_CONTROL,
                               "SpewControl",
                               "Spew control",
                               FALSE,
                               G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_NUL_CONTROL,
         g_param_spec_boolean (MM_PORT_SERIAL_NUL_CONTROL,
                               "NulControl",
                               "Nul control",
                               FALSE,
                               G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_RTS_CTS,
         g_param_spec_boolean (MM_PORT_SERIAL_RTS_CTS,
                               "RTSCTS",
                               "Enable RTS/CTS flow control",
                               FALSE,
                               G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_FLASH_OK,
         g_param_spec_boolean (MM_PORT_SERIAL_FLASH_OK,
                               "FlashOk",
                               "Flashing the port (0 baud for a short period) "
                               "is allowed.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /* Signals */
    signals[BUFFER_FULL] =
        g_signal_new ("buffer-full",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMPortSerialClass, buffer_full),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[TIMED_OUT] =
        g_signal_new ("timed-out",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMPortSerialClass, timed_out),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 1, G_TYPE_UINT);

    signals[FORCED_CLOSE] =
        g_signal_new ("forced-close",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMPortSerialClass, forced_close),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 0);
}
