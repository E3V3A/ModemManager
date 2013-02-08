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

#ifndef LIBQCDM_LOG_ITEMS_H
#define LIBQCDM_LOG_ITEMS_H

#include "utils.h"
#include "result.h"

/**********************************************************************/

/* Generic fields of the log item header */
#define QCDM_LOG_ITEM_CODE      "code"
#define QCDM_LOG_ITEM_TIMESTAMP "timestamp"

/* log_code = 0 is a special case for verifying basic log item structure */
qcdmbool qcdm_is_log_item (const char *buf, size_t len, u_int16_t log_code);

/* 0 on error or if buf does not contain a log item */
u_int16_t qcdm_get_log_item_code (const char *buf, size_t len);

/**********************************************************************/

enum {
    QCDM_LOG_ITEM_PILOT_SET_TYPE_UNKNOWN = 0,
    QCDM_LOG_ITEM_PILOT_SET_TYPE_ACTIVE = 1,
    QCDM_LOG_ITEM_PILOT_SET_TYPE_CANDIDATE = 2,
    QCDM_LOG_ITEM_PILOT_SET_TYPE_NEIGHBOR = 3,
};

#define QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_PN_OFFSET            "pn-offset"
#define QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_ACTIVE_SET_WINDOW    "active-set-window"
#define QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_CANDIDATE_SET_WINDOW "candidate-set-window"
#define QCDM_LOG_ITEM_EVDO_PILOT_SETS_V2_NEIGHBOR_SET_WINDOW  "neighbor-set-window"

QcdmResult *qcdm_log_item_evdo_pilot_sets_v2_result (const char *buf,
                                                     size_t len,
                                                     int *out_error);

qcdmbool    qcdm_log_item_evdo_pilot_sets_v2_result_get_num   (QcdmResult *result,
                                                               u_int32_t set_type,
                                                               u_int32_t *out_num);

qcdmbool    qcdm_log_item_evdo_pilot_sets_result_get_pilot (QcdmResult *result,
                                                            u_int32_t set_type,
                                                            u_int32_t num,
                                                            u_int32_t *out_pilot_pn,
                                                            u_int32_t *out_energy,
                                                            int32_t *out_dbm,
                                                            u_int32_t *out_band_class,
                                                            u_int32_t *out_channel,
                                                            u_int32_t *out_window_center);

/**********************************************************************/

#endif  /* LIBQCDM_LOG_ITEMS_H */
