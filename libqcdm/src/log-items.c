/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <endian.h>

#include "log-items.h"
#include "errors.h"
#include "dm-commands.h"
#include "dm-log-items.h"
#include "result-private.h"
#include "utils.h"

/**********************************************************************/

static qcdmbool
check_log_item (const char *buf, size_t len, u_int16_t log_code, size_t min_len, int *out_error)
{
    DMCmdLog *log = (DMCmdLog *) buf;

    if (len < sizeof (DMCmdLog)) {
        qcdm_err (0, "DM command response malformed (must be at least 1 byte in length)");
        if (out_error)
            *out_error = -QCDM_ERROR_RESPONSE_MALFORMED;
        return FALSE;
    }

    /* Make sure it's a log item */
    if (log->code != 0x10) {
        qcdm_err (0, "Not a log item (expected 0x10, got %d)", log->code);
        if (out_error)
            *out_error = -QCDM_ERROR_RESPONSE_UNEXPECTED;
        return FALSE;
    }

    /* Make sure it's the log item we want */
    if (log_code && (log->log_code != log_code)) {
        qcdm_err (0, "Not the expected log item (expected %d, got %d)", log_code, log->log_code);
        if (out_error)
            *out_error = -QCDM_ERROR_RESPONSE_UNEXPECTED;
        return FALSE;
    }

    if (len < min_len) {
        qcdm_err (0, "Log item %d response not long enough (got %zu, expected "
                  "at least %zu).", log_code, len, min_len);
        if (out_error)
            *out_error = -QCDM_ERROR_RESPONSE_BAD_LENGTH;
        return FALSE;
    }

    return TRUE;
}

qcdmbool
qcdm_is_log_item (const char *buf, size_t len, u_int16_t log_code)
{
    return check_log_item (buf, len, log_code, sizeof (DMCmdLog), NULL);
}

u_int16_t
qcdm_get_log_item_code (const char *buf, size_t len)
{
    if (check_log_item (buf, len, 0, sizeof (DMCmdLog), NULL))
        return le16toh (((DMCmdLog *) buf)->log_code);
    return 0;
}

/**********************************************************************/

