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
#include "mm-rawip-serial-port.h"

G_DEFINE_TYPE (MMBroadbandBearerInfineon, mm_broadband_bearer_infineon, MM_TYPE_BROADBAND_BEARER)

struct _MMBroadbandBearerInfineonPrivate {
    MMBearerIpConfig *current_ip_config;
    gchar *rawip_device_name;
    MMRawipSerialPort *rawip;
    MMPort *rawip_device;
};

/*****************************************************************************/
/* 3GPP Dialing (sub-step of the 3GPP Connection sequence) */

typedef struct {
    GSimpleAsyncResult *result;
    MMBroadbandBearerInfineon *self;
    MMBaseModem *modem;
    MMAtSerialPort *primary;
    guint cid;
    GCancellable *cancellable;
    gboolean close_data_on_exit;
    MMBearerIpConfig *ip_config;
} Dial3gppContext;

static void
dial_3gpp_context_complete_and_free (Dial3gppContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    if (ctx->close_data_on_exit)
        mm_serial_port_close (MM_SERIAL_PORT (ctx->primary));
    if (ctx->ip_config)
        g_object_unref (ctx->ip_config);
    g_object_unref (ctx->cancellable);
    g_object_unref (ctx->result);
    g_object_unref (ctx->primary);
    g_object_unref (ctx->modem);
    g_object_unref (ctx->self);
    g_slice_free (Dial3gppContext, ctx);
}

static gboolean
dial_3gpp_context_set_error_if_cancelled (Dial3gppContext *ctx,
                                          GError **error)
{
    if (!g_cancellable_is_cancelled (ctx->cancellable))
        return FALSE;

    g_set_error (error,
                 MM_CORE_ERROR,
                 MM_CORE_ERROR_CANCELLED,
                 "Dial operation has been cancelled");
    return TRUE;
}

static gboolean
dial_3gpp_context_complete_and_free_if_cancelled (Dial3gppContext *ctx)
{
    GError *error = NULL;

    if (!dial_3gpp_context_set_error_if_cancelled (ctx, &error))
        return FALSE;

    g_simple_async_result_take_error (ctx->result, error);
    dial_3gpp_context_complete_and_free (ctx);
    return TRUE;
}

static MMPort *
dial_3gpp_finish (MMBroadbandBearer *self,
                  GAsyncResult *res,
                  GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return MM_PORT (g_object_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res))));
}

static gboolean
parse_xdns_query_response (const gchar *reply,
                           guint *cid,
                           gchar **dns1,
                           gchar **dns2)
{
    GRegex *r;
    GMatchInfo *match_info;
    gboolean success = FALSE;

    *dns1 = NULL;
    *dns2 = NULL;

    r = g_regex_new ("\\+XDNS:\\s*(\\d+)\\s*,\\s*\"(.*)\"\\s*,\\s*\"(.*)\"",
                     G_REGEX_OPTIMIZE | G_REGEX_RAW,
                     0, NULL);
    g_assert (r != NULL);

    if (g_regex_match (r, reply, 0, &match_info)) {
        if (mm_get_uint_from_match_info (match_info, 1, cid) &&
            (*dns1 = mm_get_string_unquoted_from_match_info (match_info, 2)) != NULL &&
            (*dns2 = mm_get_string_unquoted_from_match_info (match_info, 3)) != NULL)
            success = TRUE;
    }
    g_match_info_free (match_info);
    g_regex_unref (r);

    if (!success) {
        g_free (*dns1);
        g_free (*dns2);
    }

    return success;
}

static gchar *
create_tun_device_name (MMBroadbandBearerInfineon *self)
{
    const gchar *str;
    gchar *path;
    guint index;

    /* Just pick the bearer index from the path, and add it to the name */
    g_object_get (self,
                  MM_BEARER_PATH, &path,
                  NULL);
    g_assert (path);

    str = g_strrstr (path, "/");
    g_assert (str != NULL);
    str++;

    g_assert (mm_get_uint_from_str (str, &index));
    g_free (path);

    return g_strdup_printf ("bearer%u", index);
}

static void
data_ready (MMBaseModem *modem,
            GAsyncResult *res,
            Dial3gppContext *ctx)
{
    GError *error = NULL;
    gchar *tty_device;

    /* If cancelled, complete */
    if (dial_3gpp_context_complete_and_free_if_cancelled (ctx))
        return;

    if (!mm_base_modem_at_command_full_finish (modem, res, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    /* Don't close on exit */
    ctx->close_data_on_exit = FALSE;

    /* Store the IP config */
    g_warn_if_fail (ctx->self->priv->current_ip_config == NULL);
    ctx->self->priv->current_ip_config = g_object_ref (ctx->ip_config);

    /* Create TUN device */
    tty_device = g_strdup_printf ("/dev/%s",
                                  mm_port_get_device (MM_PORT (mm_base_modem_peek_port_primary (modem))));
    g_free (ctx->self->priv->rawip_device_name);
    ctx->self->priv->rawip_device_name = create_tun_device_name (ctx->self);
    mm_dbg ("Creating TUN device '%s<->%s'...", tty_device, ctx->self->priv->rawip_device_name);
    ctx->self->priv->rawip_device = g_object_new (MM_TYPE_PORT,
                                                  MM_PORT_DEVICE, ctx->self->priv->rawip_device_name,
                                                  MM_PORT_SUBSYS, MM_PORT_SUBSYS_NET,
                                                  MM_PORT_TYPE, MM_PORT_TYPE_NET,
                                                  NULL);
    ctx->self->priv->rawip = mm_rawip_serial_port_new (tty_device, ctx->self->priv->rawip_device_name);
    g_free (tty_device);

    if (!mm_serial_port_open (MM_SERIAL_PORT (ctx->self->priv->rawip), &error)) {
        g_clear_object (&ctx->self->priv->rawip_device);
        g_clear_object (&ctx->self->priv->rawip);
        g_simple_async_result_take_error (ctx->result, error);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    g_simple_async_result_set_op_res_gpointer (ctx->result,
                                               g_object_ref (ctx->self->priv->rawip_device),
                                               g_object_unref);
    dial_3gpp_context_complete_and_free (ctx);
}

static void
dns_info_ready (MMBaseModem *modem,
                GAsyncResult *res,
                Dial3gppContext *ctx)
{
    const gchar *response;
    GError *error = NULL;
    gchar *command;
    guint cid;
    gchar *dns[3];

    /* If cancelled, complete */
    if (dial_3gpp_context_complete_and_free_if_cancelled (ctx))
        return;

    response = mm_base_modem_at_command_full_finish (modem, res, &error);
    if (!response) {
        g_simple_async_result_take_error (ctx->result, error);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    /* Parse response */
    if (!parse_xdns_query_response (response, &cid, &dns[0], &dns[1])) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "Error parsing +XDNS response: '%s'",
                                         response);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    g_warn_if_fail (cid == ctx->cid);

    dns[2] = NULL;
    mm_bearer_ip_config_set_dns (ctx->ip_config, (const gchar **)dns);

    /* Success, launch data */
    command = g_strdup_printf ("AT+CGDATA=\"M-RAW_IP\",%u", ctx->cid);
    mm_base_modem_at_command_full (ctx->modem,
                                   ctx->primary,
                                   command,
                                   3,
                                   FALSE,
                                   FALSE, /* raw */
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback)data_ready,
                                   ctx);
    g_free (command);
}

static void
ip_info_ready (MMBaseModem *modem,
               GAsyncResult *res,
               Dial3gppContext *ctx)
{
    const gchar *response;
    GError *error = NULL;
    guint cid;
    gchar *ip;

    /* If cancelled, complete */
    if (dial_3gpp_context_complete_and_free_if_cancelled (ctx))
        return;

    response = mm_base_modem_at_command_full_finish (modem, res, &error);
    if (!response) {
        g_simple_async_result_take_error (ctx->result, error);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    /* Parse response */
    if (!mm_3gpp_parse_cgpaddr_write_response (response, &cid, &ip)) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "Error parsing +CGPADDR response: '%s'",
                                         response);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    g_warn_if_fail (cid == ctx->cid);

    /* Create IP config */
    ctx->ip_config = mm_bearer_ip_config_new ();
    mm_bearer_ip_config_set_method (ctx->ip_config, MM_BEARER_IP_METHOD_STATIC);
    mm_bearer_ip_config_set_address (ctx->ip_config, ip);
    mm_bearer_ip_config_set_prefix (ctx->ip_config, 0);
    g_free (ip);

    /* Success, query DNS addresses */
    mm_base_modem_at_command_full (ctx->modem,
                                   ctx->primary,
                                   "AT+XDNS?",
                                   3,
                                   FALSE,
                                   FALSE, /* raw */
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback)dns_info_ready,
                                   ctx);
}

static void
activate_ready (MMBaseModem *modem,
                GAsyncResult *res,
                Dial3gppContext *ctx)
{
    GError *error = NULL;
    gchar *command;

    /* If cancelled, complete */
    if (dial_3gpp_context_complete_and_free_if_cancelled (ctx))
        return;

    if (!mm_base_modem_at_command_full_finish (modem, res, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    /* Success, query address */
    command = g_strdup_printf ("AT+CGPADDR=%u", ctx->cid);
    mm_base_modem_at_command_full (ctx->modem,
                                   ctx->primary,
                                   command,
                                   3,
                                   FALSE,
                                   FALSE, /* raw */
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback)ip_info_ready,
                                   ctx);
    g_free (command);
}

static void
ip_type_ready (MMBaseModem *modem,
               GAsyncResult *res,
               Dial3gppContext *ctx)
{
    GError *error = NULL;
    gchar *command;

    /* If cancelled, complete */
    if (dial_3gpp_context_complete_and_free_if_cancelled (ctx))
        return;

    if (!mm_base_modem_at_command_full_finish (modem, res, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    /* Success, activate context */
    command = g_strdup_printf ("AT+CGACT=1,%u", ctx->cid);
    mm_base_modem_at_command_full (ctx->modem,
                                   ctx->primary,
                                   command,
                                   3,
                                   FALSE,
                                   FALSE, /* raw */
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback)activate_ready,
                                   ctx);
    g_free (command);
}

static void
authenticate_ready (MMBaseModem *modem,
                    GAsyncResult *res,
                    Dial3gppContext *ctx)
{
    GError *error = NULL;
    gchar *command = NULL;

    /* If cancelled, complete */
    if (dial_3gpp_context_complete_and_free_if_cancelled (ctx))
        return;

    if (!mm_base_modem_at_command_full_finish (modem, res, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        dial_3gpp_context_complete_and_free (ctx);
        return;
    }

    /* Success, select IP type */
    switch (mm_bearer_properties_get_ip_type (mm_bearer_peek_config (MM_BEARER (ctx->self)))) {
    case MM_BEARER_IP_FAMILY_IPV4:
        command = g_strdup_printf ("AT+XDNS=%u,1", ctx->cid);
        break;
    case MM_BEARER_IP_FAMILY_IPV6:
        command = g_strdup_printf ("AT+XDNS=%u,2", ctx->cid);
        break;
    case MM_BEARER_IP_FAMILY_IPV4V6:
        command = g_strdup_printf ("AT+XDNS=%u,3", ctx->cid);
        break;
    default:
        g_warn_if_reached ();
        break;
    }

    mm_base_modem_at_command_full (ctx->modem,
                                   ctx->primary,
                                   command,
                                   3,
                                   FALSE,
                                   FALSE, /* raw */
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback)ip_type_ready,
                                   ctx);
    g_free (command);
}

static void
authenticate (Dial3gppContext *ctx)
{
    gchar *command;
    const gchar *user;
    const gchar *password;

    user = mm_bearer_properties_get_user (mm_bearer_peek_config (MM_BEARER (ctx->self)));
    password = mm_bearer_properties_get_password (mm_bearer_peek_config (MM_BEARER (ctx->self)));

    /* Both user and password are required; otherwise firmware returns an error */
    if (user || password) {
        gchar *encoded_user;
        gchar *encoded_password;

        encoded_user = mm_broadband_modem_take_and_convert_to_current_charset (MM_BROADBAND_MODEM (ctx->modem),
                                                                               g_strdup (user));
        encoded_password = mm_broadband_modem_take_and_convert_to_current_charset (MM_BROADBAND_MODEM (ctx->modem),
                                                                                   g_strdup (password));

        command = g_strdup_printf ("AT+XGAUTH=%u,1,\"%s\",\"%s\"",
                                   ctx->cid,
                                   encoded_user ? encoded_user : "",
                                   encoded_password ? encoded_password : "");
        g_free (encoded_user);
        g_free (encoded_password);
    } else
        command = g_strdup_printf ("AT+XGAUTH=%u,0,\"\",\"\"", ctx->cid);

    mm_base_modem_at_command_full (ctx->modem,
                                   ctx->primary,
                                   command,
                                   3,
                                   FALSE,
                                   FALSE, /* raw */
                                   NULL, /* cancellable */
                                   (GAsyncReadyCallback)authenticate_ready,
                                   ctx);
    g_free (command);
}

static void
dial_3gpp (MMBroadbandBearer *self,
           MMBaseModem *modem,
           MMAtSerialPort *primary,
           guint cid,
           GCancellable *cancellable,
           GAsyncReadyCallback callback,
           gpointer user_data)
{
    Dial3gppContext *ctx;

    g_assert (primary != NULL);

    ctx = g_slice_new0 (Dial3gppContext);
    ctx->self = g_object_ref (self);
    ctx->modem = g_object_ref (modem);
    ctx->primary = g_object_ref (primary);
    ctx->cid = cid;
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             dial_3gpp);
    ctx->cancellable = g_object_ref (cancellable);

    /* Use primary */
    mm_serial_port_open (MM_SERIAL_PORT (ctx->primary), NULL);
    ctx->close_data_on_exit = TRUE;

    authenticate (ctx);
}

/*****************************************************************************/
/* 3GPP IP config retrieval (sub-step of the 3GPP Connection sequence) */

static gboolean
get_ip_config_3gpp_finish (MMBroadbandBearer *self,
                           GAsyncResult *res,
                           MMBearerIpConfig **ipv4_config,
                           MMBearerIpConfig **ipv6_config,
                           GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return FALSE;

    *ipv4_config = g_object_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
    /* TODO: clearly not ok. How do we get IPv6? */
    *ipv6_config = NULL;
    return TRUE;
}

static void
get_ip_config_3gpp (MMBroadbandBearer *_self,
                    MMBroadbandModem *modem,
                    MMAtSerialPort *primary,
                    MMAtSerialPort *secondary,
                    MMPort *data,
                    guint cid,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
    MMBroadbandBearerInfineon *self = MM_BROADBAND_BEARER_INFINEON (_self);
    GSimpleAsyncResult *result;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        get_ip_config_3gpp);
    if (!self->priv->current_ip_config)
        g_simple_async_result_set_error (result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "No current IP config found");
    else
        g_simple_async_result_set_op_res_gpointer (result,
                                                   self->priv->current_ip_config,
                                                   g_object_unref);

    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

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
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_BROADBAND_BEARER_INFINEON,
                                              MMBroadbandBearerInfineonPrivate);
}

static void
dispose (GObject *object)
{
    MMBroadbandBearerInfineon *self = MM_BROADBAND_BEARER_INFINEON (object);

    g_clear_object (&self->priv->current_ip_config);
    g_clear_object (&self->priv->rawip_device);
    g_clear_object (&self->priv->rawip);

    G_OBJECT_CLASS (mm_broadband_bearer_infineon_parent_class)->dispose (object);
}

static void
mm_broadband_bearer_infineon_class_init (MMBroadbandBearerInfineonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBroadbandBearerClass *broadband_bearer_class = MM_BROADBAND_BEARER_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBroadbandBearerInfineonPrivate));

    /* Virtual methods */
    object_class->dispose = dispose;

    broadband_bearer_class->dial_3gpp = dial_3gpp;
    broadband_bearer_class->dial_3gpp_finish = dial_3gpp_finish;
    broadband_bearer_class->get_ip_config_3gpp = get_ip_config_3gpp;
    broadband_bearer_class->get_ip_config_3gpp_finish = get_ip_config_3gpp_finish;
}
