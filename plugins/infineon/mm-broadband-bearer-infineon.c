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
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-base-modem-at.h"
#include "mm-broadband-bearer-infineon.h"
#include "mm-log.h"
#include "mm-modem-helpers.h"

G_DEFINE_TYPE (MMBroadbandBearerInfineon, mm_broadband_bearer_infineon, MM_TYPE_BROADBAND_BEARER)

/*****************************************************************************/

MMBearer *
mm_broadband_bearer_infineon_new_finish (GAsyncResult *res,
                                         GError **error)
{
    GObject *bearer;
    GObject *source;

    source = g_async_result_get_source_object (res);
    bearer = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, error);
    g_object_unref (source);

    if (!bearer)
        return NULL;

    /* Only export valid bearers */
    mm_bearer_export (MM_BEARER (bearer));

    return MM_BEARER (bearer);
}

void
mm_broadband_bearer_infineon_new (MMBroadbandModemInfineon *modem,
                                  MMBearerProperties *config,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    g_async_initable_new_async (
        MM_TYPE_BROADBAND_BEARER_INFINEON,
        G_PRIORITY_DEFAULT,
        cancellable,
        callback,
        user_data,
        MM_BEARER_MODEM, modem,
        MM_BEARER_CONFIG, config,
        NULL);
}

static void
mm_broadband_bearer_infineon_init (MMBroadbandBearerInfineon *self)
{
}

static void
mm_broadband_bearer_infineon_class_init (MMBroadbandBearerInfineonClass *klass)
{
}
