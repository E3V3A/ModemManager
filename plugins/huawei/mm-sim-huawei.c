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
 * Copyright (C) 2009 - 2012 Red Hat, Inc.
 * Copyright (C) 2012 Lanedo GmbH
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>
#include "mm-log.h"
#include "mm-modem-helpers.h"
#include "mm-base-modem-at.h"

#include "mm-sim-huawei.h"

G_DEFINE_TYPE (MMSimHuawei, mm_sim_huawei, MM_TYPE_SIM);

/*****************************************************************************/
/* SIM identifier loading */

static void
parent_load_sim_identifier_ready (MMSimHuawei *self,
                                  GAsyncResult *res,
                                  GSimpleAsyncResult *simple)
{
    GError *error = NULL;
    gchar *simid;

    simid = MM_SIM_CLASS (mm_sim_huawei_parent_class)->load_sim_identifier_finish (MM_SIM (self), res, &error);
    if (simid)
        g_simple_async_result_set_op_res_gpointer (simple, simid, g_free);
    else
        g_simple_async_result_take_error (simple, error);

    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static gchar *
load_sim_identifier_finish (MMSim *self,
                            GAsyncResult *res,
                            GError **error)
{
    gchar *iccid;

    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    iccid = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
    mm_dbg ("loaded SIM identifier: %s", iccid);
    return g_strdup (iccid);
}

static void
iccid_read_ready (MMBaseModem *modem,
                  GAsyncResult *res,
                  GSimpleAsyncResult *simple)
{
    MMSim *self;
    const gchar *response;
    const gchar *p;
    char *parsed;

    response = mm_base_modem_at_command_finish (modem, res, NULL);
    if (!response)
        goto error;

    p = mm_strip_tag (response, "^ICCID:");
    if (!p)
        goto error;

    /* Huawei ^ICCID response must be character swapped */
    parsed = mm_3gpp_parse_iccid (p, TRUE, NULL);
    if (parsed) {
        g_simple_async_result_set_op_res_gpointer (simple, parsed, g_free);
        g_simple_async_result_complete (simple);
        g_object_unref (simple);
        return;
    }

error:
    /* Chain up to parent method; older devices don't support ^ICCID */
    self = MM_SIM (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
    MM_SIM_CLASS (mm_sim_huawei_parent_class)->load_sim_identifier (self,
                                                                    (GAsyncReadyCallback) parent_load_sim_identifier_ready,
                                                                    simple);
}

static void
load_sim_identifier (MMSim *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    MMBaseModem *modem = NULL;

    g_object_get (self,
                  MM_SIM_MODEM, &modem,
                  NULL);

    mm_dbg ("loading (Huawei) SIM identifier...");
    mm_base_modem_at_command (
        MM_BASE_MODEM (modem),
        "^ICCID?",
        5,
        FALSE,
        (GAsyncReadyCallback)iccid_read_ready,
        g_simple_async_result_new (G_OBJECT (self),
                                   callback,
                                   user_data,
                                   load_sim_identifier));
    g_object_unref (modem);
}

/*****************************************************************************/

MMSim *
mm_sim_huawei_new_finish (GAsyncResult  *res,
                          GError       **error)
{
    GObject *source;
    GObject *sim;

    source = g_async_result_get_source_object (res);
    sim = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, error);
    g_object_unref (source);

    if (!sim)
        return NULL;

    /* Only export valid SIMs */
    mm_sim_export (MM_SIM (sim));

    return MM_SIM (sim);
}

void
mm_sim_huawei_new (MMBaseModem *modem,
                   GCancellable *cancellable,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
    g_async_initable_new_async (MM_TYPE_SIM_HUAWEI,
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                callback,
                                user_data,
                                MM_SIM_MODEM, modem,
                                NULL);
}

static void
mm_sim_huawei_init (MMSimHuawei *self)
{
}

static void
mm_sim_huawei_class_init (MMSimHuaweiClass *klass)
{
    MMSimClass *sim_class = MM_SIM_CLASS (klass);

    sim_class->load_sim_identifier = load_sim_identifier;
    sim_class->load_sim_identifier_finish = load_sim_identifier_finish;
}
