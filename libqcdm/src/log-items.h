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

#endif  /* LIBQCDM_LOG_ITEMS_H */
