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
#include <sys/param.h>

#include "log-items.h"
#include "errors.h"
#include "dm-commands.h"
#include "dm-log-items.h"
#include "result-private.h"
#include "commands-private.h"
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

#define ACTIVE_CHANNEL "active-channel"

QcdmResult *
qcdm_log_item_evdo_pilot_sets_v2_result (const char *buf, size_t len, int *out_error)
{
    QcdmResult *result = NULL;
    DMLogItemEvdoPilotSetsV2 *log = (DMLogItemEvdoPilotSetsV2 *) buf;
    size_t sets_len;

    qcdm_return_val_if_fail (buf != NULL, NULL);

    if (!check_log_item (buf, len, DM_LOG_ITEM_EVDO_PILOT_SETS_V2, sizeof (DMLogItemEvdoPilotSetsV2), out_error))
        return NULL;

    result = qcdm_result_new ();

    qcdm_result_add_u32 (result, QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_PN_OFFSET, log->pn_offset);
    qcdm_result_add_u32 (result, ACTIVE_CHANNEL, le16toh (log->active_channel));
    qcdm_result_add_u32 (result, QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_ACTIVE_SET_WINDOW, log->active_window);
    qcdm_result_add_u32 (result, QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_CANDIDATE_SET_WINDOW, log->candidate_window);
    qcdm_result_add_u32 (result, QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_NEIGHBOR_SET_WINDOW, log->neighbor_window);

    sets_len = log->active_count * sizeof (DMLogItemEvdoPilotSetsV2PilotActive);
    if (sets_len > 0) {
        qcdm_result_add_u8_array (result,
                                  PILOT_SET_ACTIVE,
                                  (const u_int8_t *) &log->sets[0],
                                  sets_len);
    }

    sets_len = log->candidate_count * sizeof (DMLogItemEvdoPilotSetsV2PilotCandidate);
    if (sets_len > 0) {
        qcdm_result_add_u8_array (result,
                                  PILOT_SET_CANDIDATE,
                                  (const u_int8_t *) &log->sets[log->active_count],
                                  sets_len);
    }

    sets_len = log->neighbor_count * sizeof (DMLogItemEvdoPilotSetsV2PilotNeighbor);
    if (sets_len > 0) {
        qcdm_result_add_u8_array (result,
                                  PILOT_SET_NEIGHBOR,
                                  (const u_int8_t *) &log->sets[log->active_count + log->candidate_count],
                                  sets_len);
    }

    return result;
}

qcdmbool
qcdm_log_item_evdo_pilot_sets_v2_result_get_num (QcdmResult *result,
                                                 u_int32_t set_type,
                                                 u_int32_t *out_num)
{
    const char *set_name;
    const u_int8_t *array = NULL;
    size_t array_len = 0;

    qcdm_return_val_if_fail (result != NULL, FALSE);

    set_name = pilot_set_num_to_str (set_type);
    qcdm_return_val_if_fail (set_name != NULL, FALSE);

    if (qcdm_result_get_u8_array (result, set_name, &array, &array_len))
        return FALSE;

    /* we use Active here, but all the pilot type structs are the same size */
    *out_num = array_len / sizeof (DMLogItemEvdoPilotSetsV2PilotActive);
    return TRUE;
}

static int32_t
round_to_int (float in)
{
    if (in < 0)
        return (int32_t) (in - 0.5);
    else
        return (int32_t) (in + 0.5);
}

qcdmbool
qcdm_log_item_evdo_pilot_sets_result_get_pilot (QcdmResult *result,
                                                u_int32_t set_type,
                                                u_int32_t num,
                                                u_int32_t *out_pilot_pn,
                                                u_int32_t *out_energy,
                                                int32_t *out_dbm,
                                                u_int32_t *out_band_class,
                                                u_int32_t *out_channel,
                                                u_int32_t *out_window_center)
{
    const char *set_name;
    DMLogItemEvdoPilotSetsV2Pilot *set;
    const u_int8_t *array = NULL;
    size_t array_len = 0;
    u_int32_t pilot_pn;
    u_int32_t energy;
    u_int32_t channel = 0;
    u_int32_t window_center;

    qcdm_return_val_if_fail (result != NULL, FALSE);

    set_name = pilot_set_num_to_str (set_type);
    qcdm_return_val_if_fail (set_name != NULL, FALSE);

    if (qcdm_result_get_u8_array (result, set_name, &array, &array_len))
        return FALSE;

    qcdm_return_val_if_fail (num < array_len / sizeof (DMLogItemEvdoPilotSetsV2Pilot), FALSE);

    set = (DMLogItemEvdoPilotSetsV2Pilot *) &array[num * sizeof (DMLogItemEvdoPilotSetsV2Pilot)];
    if (set_type == QCDM_PILOT_SET_TYPE_ACTIVE) {
        pilot_pn = le16toh (set[num].u.active.pilot_pn);
        energy = le16toh (set[num].u.active.energy);
        qcdm_result_get_u32 (result, ACTIVE_CHANNEL, &channel);
        window_center = le16toh (set[num].u.active.window_center);
    } else if (set_type == QCDM_PILOT_SET_TYPE_CANDIDATE) {
        pilot_pn = le16toh (set[num].u.candidate.pilot_pn);
        energy = le16toh (set[num].u.candidate.energy);
        channel = le16toh (set[num].u.candidate.channel);
        window_center = le16toh (set[num].u.candidate.window_center);
    } else if (set_type == QCDM_PILOT_SET_TYPE_NEIGHBOR) {
        pilot_pn = le16toh (set[num].u.neighbor.pilot_pn);
        energy = le16toh (set[num].u.neighbor.energy);
        channel = le16toh (set[num].u.neighbor.channel);
        window_center = le16toh (set[num].u.neighbor.window_center);
    } else
        qcdm_assert_not_reached ();

    if (out_pilot_pn)
        *out_pilot_pn = pilot_pn;
    if (out_band_class)
        *out_band_class = cdma_band_class_to_qcdm (DM_LOG_ITEM_EVDO_PILOT_SETS_V2_GET_BAND_CLASS (channel));
    if (out_channel)
        *out_channel = DM_LOG_ITEM_EVDO_PILOT_SETS_V2_GET_CHANNEL (channel);
    if (out_window_center)
        *out_window_center = window_center;

    /* Log item pilot energy is in units from 0 (bad signal) to 500 (excellent
     * signal).  Correlating to dBm from WMC and Novatel snapshots, this yields
     * the following approximate equation to convert pilot energy to RSSI dBm:
     *
     * dBm = ((pilot energy * 25) / 500) - 100
     */
    if (out_energy)
        *out_energy = energy;

    if (out_dbm)
        *out_dbm = round_to_int (((float) (energy * 25) / 500.0) - 100);

    return TRUE;
}

/**********************************************************************/

