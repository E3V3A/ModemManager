/* -*- Mode: C; tab-width: 4; indent-tab-mode: nil; c-basic-offset: 4 -*- */
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

#include <ModemManager.h>
#include <mm-errors-types.h>

#include "mm-serial-port.h"
#include "mm-log.h"

G_DEFINE_TYPE (MMSerialPort, mm_serial_port, MM_TYPE_PORT)

enum {
    PROP_0,
    PROP_BAUD,
    PROP_BITS,
    PROP_PARITY,
    PROP_STOPBITS,
    PROP_FD,
    PROP_RTS_CTS,
    PROP_FLASH_OK,
    PROP_LAST
};

enum {
    SIGNAL_FORCED_CLOSE,
    SIGNAL_LAST
};

struct _MMSerialPortPrivate {
    guint32 open_count;
    gboolean forced_close;
    gint fd;

    struct termios old_t;

    guint baud;
    guint bits;
    char parity;
    guint stopbits;
    gboolean rts_cts;
    gboolean flash_ok;

    guint flash_id;
    guint reopen_id;
};

static guint signals[SIGNAL_LAST] = { 0 };

/*****************************************************************************/
/* Port config printing (disabled) */

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
mm_serial_port_print_config (MMSerialPort *port, const char *detail)
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

/*****************************************************************************/
/* FD config */

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
real_config_fd (MMSerialPort *self, int fd, GError **error)
{
    struct termios stbuf, other;
    int speed;
    int bits;
    int parity;
    int stopbits;

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

/*****************************************************************************/
/* Port open */

gboolean
mm_serial_port_open (MMSerialPort *self, GError **error)
{
    char *devfile;
    const char *device;
    struct serial_struct sinfo = { 0 };
    GTimeVal tv_start, tv_end;

    g_return_val_if_fail (MM_IS_SERIAL_PORT (self), FALSE);

    device = mm_port_get_device (MM_PORT (self));

    if (self->priv->reopen_id) {
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

    /* Only open a new file descriptor if we weren't given one already */
    if (self->priv->fd < 0) {
        devfile = g_strdup_printf ("/dev/%s", device);
        errno = 0;
        self->priv->fd = open (devfile, O_RDWR | O_EXCL | O_NONBLOCK | O_NOCTTY);
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
                     "Could not open serial device %s: %s", device, strerror (errno));
        return FALSE;
    }

    if (ioctl (self->priv->fd, TIOCEXCL) < 0) {
        g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_OPEN_FAILED,
                     "Could not lock serial device %s: %s", device, strerror (errno));
        goto error;
    }

    /* Flush any waiting IO */
    tcflush (self->priv->fd, TCIOFLUSH);

    if (tcgetattr (self->priv->fd, &self->priv->old_t) < 0) {
        g_set_error (error, MM_SERIAL_ERROR, MM_SERIAL_ERROR_OPEN_FAILED,
                     "Could not open serial device %s: %s", device, strerror (errno));
        goto error;
    }

    g_warn_if_fail (MM_SERIAL_PORT_GET_CLASS (self)->config_fd);
    if (!MM_SERIAL_PORT_GET_CLASS (self)->config_fd (self, self->priv->fd, error))
        goto error;

    /* Don't wait for pending data when closing the port; this can cause some
     * stupid devices that don't respond to URBs on a particular port to hang
     * for 30 seconds when probing fails.  See GNOME bug #630670.
     */
    if (ioctl (self->priv->fd, TIOCGSERIAL, &sinfo) == 0) {
        sinfo.closing_wait = ASYNC_CLOSING_WAIT_NONE;
        ioctl (self->priv->fd, TIOCSSERIAL, &sinfo);
    }

    g_get_current_time (&tv_end);

    if (tv_end.tv_sec - tv_start.tv_sec > 7)
        mm_warn ("(%s): open blocked by driver for more than 7 seconds!", device);

    /* Call subclass's data watch enabling */
    g_warn_if_fail (MM_SERIAL_PORT_GET_CLASS (self)->data_watch_enable);
    if (!MM_SERIAL_PORT_GET_CLASS (self)->data_watch_enable (self, self->priv->fd, error))
        goto error;

success:
    self->priv->open_count++;
    mm_dbg ("(%s) device open count is %d (open)", device, self->priv->open_count);

    /* Run additional port config if just opened */
    if (self->priv->open_count == 1 && MM_SERIAL_PORT_GET_CLASS (self)->config)
        MM_SERIAL_PORT_GET_CLASS (self)->config (self);

    return TRUE;

error:
    close (self->priv->fd);
    self->priv->fd = -1;
    return FALSE;
}

gboolean
mm_serial_port_is_open (MMSerialPort *self)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (MM_IS_SERIAL_PORT (self), FALSE);

    return !!self->priv->open_count;
}

/*****************************************************************************/
/* Port close */

void
mm_serial_port_close (MMSerialPort *self)
{
    GTimeVal tv_start, tv_end;
    struct serial_struct sinfo = { 0 };
    const char *device;

    g_return_if_fail (MM_IS_SERIAL_PORT (self));

    /* If we forced closing the port, open_count will be 0 already.
     * Just return without issuing any warning */
    if (self->priv->forced_close)
        return;

    g_return_if_fail (self->priv->open_count > 0);

    device = mm_port_get_device (MM_PORT (self));

    self->priv->open_count--;

    mm_dbg ("(%s) device open count is %d (close)", device, self->priv->open_count);

    if (self->priv->open_count > 0)
        return;

    mm_serial_port_flash_cancel (self);

    g_warn_if_fail (self->priv->fd >= 0);
    mm_dbg ("(%s) closing serial port...", device);

    /* Disable data watch in subclass */
    MM_SERIAL_PORT_GET_CLASS (self)->data_watch_enable (self, -1, NULL);

    mm_port_set_connected (MM_PORT (self), FALSE);

    /* Paranoid: ensure our closing_wait value is still set so we ignore
     * pending data when closing the port.  See GNOME bug #630670.
     */
    if (ioctl (self->priv->fd, TIOCGSERIAL, &sinfo) == 0) {
        if (sinfo.closing_wait != ASYNC_CLOSING_WAIT_NONE) {
            mm_warn ("(%s): serial port closing_wait was reset!", device);
            sinfo.closing_wait = ASYNC_CLOSING_WAIT_NONE;
            (void) ioctl (self->priv->fd, TIOCSSERIAL, &sinfo);
        }
    }

    g_get_current_time (&tv_start);

    tcsetattr (self->priv->fd, TCSANOW, &self->priv->old_t);
    tcflush (self->priv->fd, TCIOFLUSH);
    close (self->priv->fd);
    self->priv->fd = -1;

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

void
mm_serial_port_close_force (MMSerialPort *self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (MM_IS_SERIAL_PORT (self));

    /* If already forced to close, return */
    if (self->priv->forced_close)
        return;

    mm_dbg ("(%s) forced to close port", mm_port_get_device (MM_PORT (self)));

    /* If already closed, done */
    if (!self->priv->open_count)
        return;

    /* Force the port to close */
    self->priv->open_count = 1;
    mm_serial_port_close (self);

    /* Mark as having forced the close, so that we don't warn about incorrect
     * open counts */
    self->priv->forced_close = TRUE;

    /* Notify about the forced close status */
    g_signal_emit (self, signals[SIGNAL_FORCED_CLOSE], 0);
}

/*****************************************************************************/
/* Port reopening */

typedef struct {
    MMSerialPort *self;
    guint initial_open_count;
    MMSerialReopenFn callback;
    gpointer user_data;
} ReopenInfo;

static void
serial_port_reopen_cancel (MMSerialPort *self)
{
    g_return_if_fail (MM_IS_SERIAL_PORT (self));

    if (self->priv->reopen_id > 0) {
        g_source_remove (self->priv->reopen_id);
        self->priv->reopen_id = 0;
    }
}

static gboolean
reopen_do (gpointer data)
{
    ReopenInfo *info = (ReopenInfo *) data;
    GError *error = NULL;
    guint i;

    info->self->priv->reopen_id = 0;

    for (i = 0; i < info->initial_open_count; i++) {
        if (!mm_serial_port_open (info->self, &error)) {
            g_prefix_error (&error, "Couldn't reopen port (%u): ", i);
            break;
        }
    }

    info->callback (info->self, error, info->user_data);
    if (error)
        g_error_free (error);
    g_slice_free (ReopenInfo, info);
    return FALSE;
}

gboolean
mm_serial_port_reopen (MMSerialPort *self,
                       guint32 reopen_time,
                       MMSerialReopenFn callback,
                       gpointer user_data)
{
    ReopenInfo *info;
    guint i;

    g_return_val_if_fail (MM_IS_SERIAL_PORT (self), FALSE);

    if (self->priv->reopen_id > 0) {
        GError *error;

        error = g_error_new_literal (MM_CORE_ERROR,
                                     MM_CORE_ERROR_IN_PROGRESS,
                                     "Modem is already being reopened.");
        callback (self, error, user_data);
        g_error_free (error);
        return FALSE;
    }

    info = g_slice_new0 (ReopenInfo);
    info->self = self;
    info->callback = callback;
    info->user_data = user_data;

    info->initial_open_count = self->priv->open_count;

    mm_dbg ("(%s) reopening port (%u)",
            mm_port_get_device (MM_PORT (self)),
            info->initial_open_count);

    for (i = 0; i < info->initial_open_count; i++)
        mm_serial_port_close (self);

    if (reopen_time > 0)
        self->priv->reopen_id = g_timeout_add (reopen_time, reopen_do, info);
    else
        self->priv->reopen_id = g_idle_add (reopen_do, info);

    return TRUE;
}

/*****************************************************************************/
/* Port flashing */

static gboolean
get_speed (MMSerialPort *self, speed_t *speed, GError **error)
{
    struct termios options;

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
set_speed (MMSerialPort *self, speed_t speed, GError **error)
{
    struct termios options;
    int count = 4;
    gboolean success = FALSE;

    memset (&options, 0, sizeof (struct termios));
    if (tcgetattr (self->priv->fd, &options) != 0) {
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
        if (tcsetattr (self->priv->fd, TCSANOW, &options) == 0) {
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

typedef struct {
    MMSerialPort *self;
    speed_t current_speed;
    MMSerialFlashFn callback;
    gpointer user_data;
} FlashInfo;

static gboolean
flash_do (gpointer data)
{
    FlashInfo *info = (FlashInfo *) data;
    GError *error = NULL;

    info->self->priv->flash_id = 0;

    if (info->self->priv->flash_ok) {
        if (info->current_speed) {
            if (!set_speed (info->self, info->current_speed, &error))
                g_assert (error);
        } else {
            error = g_error_new_literal (MM_SERIAL_ERROR,
                                         MM_SERIAL_ERROR_FLASH_FAILED,
                                         "Failed to retrieve current speed");
        }
    }

    info->callback (info->self, error, info->user_data);
    g_clear_error (&error);
    g_slice_free (FlashInfo, info);
    return FALSE;
}

gboolean
mm_serial_port_flash (MMSerialPort *self,
                      guint32 flash_time,
                      gboolean ignore_errors,
                      MMSerialFlashFn callback,
                      gpointer user_data)
{
    FlashInfo *info = NULL;
    GError *error = NULL;
    gboolean success;

    g_return_val_if_fail (MM_IS_SERIAL_PORT (self), FALSE);
    g_return_val_if_fail (callback != NULL, FALSE);

    if (!mm_serial_port_is_open (self)) {
        error = g_error_new_literal (MM_SERIAL_ERROR,
                                     MM_SERIAL_ERROR_NOT_OPEN,
                                     "The serial port is not open.");
        goto error;
    }

    if (self->priv->flash_id > 0) {
        error = g_error_new_literal (MM_CORE_ERROR,
                                     MM_CORE_ERROR_IN_PROGRESS,
                                     "Modem is already being flashed.");
        goto error;
    }

    info = g_slice_new0 (FlashInfo);
    info->self = self;
    info->callback = callback;
    info->user_data = user_data;

    if (self->priv->flash_ok) {
        /* Grab current speed so we can reset it after flashing */
        success = get_speed (self, &info->current_speed, &error);
        if (!success && !ignore_errors)
            goto error;
        g_clear_error (&error);

        success = set_speed (self, B0, &error);
        if (!success && !ignore_errors)
            goto error;
        g_clear_error (&error);

        self->priv->flash_id = g_timeout_add (flash_time, flash_do, info);
    } else
        self->priv->flash_id = g_idle_add (flash_do, info);

    return TRUE;

error:
    callback (self, error, user_data);
    g_clear_error (&error);
    if (info)
        g_slice_free (FlashInfo, info);
    return FALSE;
}

void
mm_serial_port_flash_cancel (MMSerialPort *self)
{
    g_return_if_fail (MM_IS_SERIAL_PORT (self));

    if (self->priv->flash_id > 0) {
        g_source_remove (self->priv->flash_id);
        self->priv->flash_id = 0;
    }
}

gboolean
mm_serial_port_get_flash_ok (MMSerialPort *self)
{
    g_return_val_if_fail (MM_IS_SERIAL_PORT (self), TRUE);

    return self->priv->flash_ok;
}

/*****************************************************************************/

MMSerialPort *
mm_serial_port_new (const char *name, MMPortType ptype)
{
    return MM_SERIAL_PORT (g_object_new (MM_TYPE_SERIAL_PORT,
                                         MM_PORT_DEVICE, name,
                                         MM_PORT_SUBSYS, MM_PORT_SUBSYS_TTY,
                                         MM_PORT_TYPE, ptype,
                                         NULL));
}

static void
mm_serial_port_init (MMSerialPort *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MM_TYPE_SERIAL_PORT, MMSerialPortPrivate);

    self->priv->fd = -1;
    self->priv->baud = 57600;
    self->priv->bits = 8;
    self->priv->parity = 'n';
    self->priv->stopbits = 1;
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
    MMSerialPort *self = MM_SERIAL_PORT (object);

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
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
    MMSerialPort *self = MM_SERIAL_PORT (object);

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
    MMSerialPort *self = MM_SERIAL_PORT (object);

    mm_serial_port_close_force (self);
    serial_port_reopen_cancel (self);
    mm_serial_port_flash_cancel (self);

    G_OBJECT_CLASS (mm_serial_port_parent_class)->dispose (object);
}

static void
mm_serial_port_class_init (MMSerialPortClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMSerialPortPrivate));

    /* Virtual methods */
    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;

    klass->config_fd = real_config_fd;

    /* Properties */
    g_object_class_install_property
        (object_class, PROP_FD,
         g_param_spec_int (MM_SERIAL_PORT_FD,
                           "File descriptor",
                           "Fiel descriptor",
                           -1, G_MAXINT, -1,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property
        (object_class, PROP_BAUD,
         g_param_spec_uint (MM_SERIAL_PORT_BAUD,
                            "Baud",
                            "Baud rate",
                            0, G_MAXUINT, 57600,
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_BITS,
         g_param_spec_uint (MM_SERIAL_PORT_BITS,
                            "Bits",
                            "Bits",
                            5, 8, 8,
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_PARITY,
         g_param_spec_char (MM_SERIAL_PORT_PARITY,
                            "Parity",
                            "Parity",
                            'E', 'o', 'n',
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_STOPBITS,
         g_param_spec_uint (MM_SERIAL_PORT_STOPBITS,
                            "Stopbits",
                            "Stopbits",
                            1, 2, 1,
                            G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_RTS_CTS,
         g_param_spec_boolean (MM_SERIAL_PORT_RTS_CTS,
                               "RTSCTS",
                               "Enable RTS/CTS flow control",
                               FALSE,
                               G_PARAM_READWRITE));

    g_object_class_install_property
        (object_class, PROP_FLASH_OK,
         g_param_spec_boolean (MM_SERIAL_PORT_FLASH_OK,
                               "FlashOk",
                               "Flashing the port (0 baud for a short period) "
                               "is allowed.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /* Signals */
    signals[SIGNAL_FORCED_CLOSE] =
        g_signal_new ("forced-close",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MMSerialPortClass, forced_close),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}
