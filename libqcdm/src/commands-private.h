/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 - 2013 Red Hat, Inc.
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

#ifndef LIBQCDM_COMMANDS_PRIVATE_H
#define LIBQCDM_COMMANDS_PRIVATE_H

#include "errors.h"
#include "utils.h"
#include "commands.h"

/**********************************************************************/

u_int8_t cdma_prev_to_qcdm (u_int8_t cdma);

u_int8_t cdma_band_class_to_qcdm (u_int8_t cdma);

#define PILOT_SET_ACTIVE    "active-set"
#define PILOT_SET_CANDIDATE "candidate-set"
#define PILOT_SET_NEIGHBOR  "neighbor-set"

const char *pilot_set_num_to_str (u_int32_t num);

/**********************************************************************/

#endif  /* LIBQCDM_COMMANDS_PRIVATE_H */
