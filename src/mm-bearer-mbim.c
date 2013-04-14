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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-modem-helpers-mbim.h"
#include "mm-serial-enums-types.h"
#include "mm-bearer-mbim.h"
#include "mm-log.h"

G_DEFINE_TYPE (MMBearerMbim, mm_bearer_mbim, MM_TYPE_BEARER)

enum {
    PROP_0,
    PROP_SESSION_ID,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _MMBearerMbimPrivate {
    /* The session ID for this bearer */
    guint32 session_id;

    MMPort *data;
};

/*****************************************************************************/

static gboolean
peek_ports (gpointer self,
            MbimDevice **o_device,
            MMPort **o_data,
            GAsyncReadyCallback callback,
            gpointer user_data)
{
    MMBaseModem *modem = NULL;

    g_object_get (G_OBJECT (self),
                  MM_BEARER_MODEM, &modem,
                  NULL);
    g_assert (MM_IS_BASE_MODEM (modem));

    if (o_device) {
        MMMbimPort *port;

        port = mm_base_modem_peek_port_mbim (modem);
        if (!port) {
            g_simple_async_report_error_in_idle (G_OBJECT (self),
                                                 callback,
                                                 user_data,
                                                 MM_CORE_ERROR,
                                                 MM_CORE_ERROR_FAILED,
                                                 "Couldn't peek MBIM port");
            g_object_unref (modem);
            return FALSE;
        }

        *o_device = mm_mbim_port_peek_device (port);
    }

    if (o_data) {
        MMPort *port;

        /* Grab a data port */
        port = mm_base_modem_peek_best_data_port (modem, MM_PORT_TYPE_NET);
        if (!port) {
            g_simple_async_report_error_in_idle (G_OBJECT (self),
                                                 callback,
                                                 user_data,
                                                 MM_CORE_ERROR,
                                                 MM_CORE_ERROR_NOT_FOUND,
                                                 "No valid data port found to launch connection");
            g_object_unref (modem);
            return FALSE;
        }

        *o_data = port;
    }

    g_object_unref (modem);
    return TRUE;
}

/*****************************************************************************/
/* Connect */

typedef enum {
    CONNECT_STEP_FIRST,
    CONNECT_STEP_PROVISIONED_CONTEXTS,
    CONNECT_STEP_CONNECT,
    CONNECT_STEP_LAST
} ConnectStep;

typedef struct {
    MMBearerMbim *self;
    MbimDevice *device;
    GSimpleAsyncResult *result;
    GCancellable *cancellable;
    MMBearerProperties *properties;
    ConnectStep step;
    MMPort *data;
    MMBearerConnectResult *connect_result;
} ConnectContext;

static void
connect_context_complete_and_free (ConnectContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    if (ctx->connect_result)
        mm_bearer_connect_result_unref (ctx->connect_result);
    g_object_unref (ctx->data);
    g_object_unref (ctx->cancellable);
    g_object_unref (ctx->device);
    g_object_unref (ctx->self);
    g_slice_free (ConnectContext, ctx);
}

static MMBearerConnectResult *
connect_finish (MMBearer *self,
                GAsyncResult *res,
                GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return mm_bearer_connect_result_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

static void connect_context_step (ConnectContext *ctx);

static void
connect_set_ready (MbimDevice *device,
                   GAsyncResult *res,
                   ConnectContext *ctx)
{
    GError *error = NULL;
    MbimMessage *response;
    guint32 session_id;
    MbimActivationState activation_state;
    MbimContextIpType ip_type;
    guint32 nw_error;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_connect_response_parse (
            response,
            &session_id,
            &activation_state,
            NULL, /* voice_call_state */
            &ip_type,
            NULL, /* context_type */
            &nw_error,
            &error)) {
        if (nw_error)
            error = mm_mobile_equipment_error_from_mbim_nw_error (nw_error);
        else {
            MMBearerIpConfig *ipv4_config = NULL;
            MMBearerIpConfig *ipv6_config = NULL;

            mm_dbg ("Session ID '%u': %s (IP type: %s)",
                    session_id,
                    mbim_activation_state_get_string (activation_state),
                    mbim_context_ip_type_get_string (ip_type));

            /* Build IPv4 config; always DHCP based */
            if (ip_type == MBIM_CONTEXT_IP_TYPE_IPV4 ||
                ip_type == MBIM_CONTEXT_IP_TYPE_IPV4V6 ||
                ip_type == MBIM_CONTEXT_IP_TYPE_IPV4_AND_IPV6) {
                ipv4_config = mm_bearer_ip_config_new ();
                mm_bearer_ip_config_set_method (ipv4_config, MM_BEARER_IP_METHOD_DHCP);
            }

            /* Build IPv6 config; always DHCP based */
            if (ip_type == MBIM_CONTEXT_IP_TYPE_IPV6 ||
                ip_type == MBIM_CONTEXT_IP_TYPE_IPV4V6 ||
                ip_type == MBIM_CONTEXT_IP_TYPE_IPV4_AND_IPV6) {
                ipv6_config = mm_bearer_ip_config_new ();
                mm_bearer_ip_config_set_method (ipv6_config, MM_BEARER_IP_METHOD_DHCP);
            }

            /* Store result */
            ctx->connect_result = (mm_bearer_connect_result_new (
                                       ctx->data,
                                       ipv4_config,
                                       ipv6_config));
        }
    }

    if (error) {
        g_simple_async_result_take_error (ctx->result, error);
        connect_context_complete_and_free (ctx);
        return;
    }

    /* Keep on */
    ctx->step++;
    connect_context_step (ctx);
}

static void
provisioned_contexts_query_ready (MbimDevice *device,
                                  GAsyncResult *res,
                                  ConnectContext *ctx)
{
    GError *error = NULL;
    MbimMessage *response;
    guint32 provisioned_contexts_count;
    MbimProvisionedContextElement **provisioned_contexts;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_provisioned_contexts_response_parse (
            response,
            &provisioned_contexts_count,
            &provisioned_contexts,
            &error)) {
        guint32 i;

        mm_dbg ("Provisioned contexts found (%u):", provisioned_contexts_count);
        for (i = 0; i < provisioned_contexts_count; i++) {
            MbimProvisionedContextElement *el = provisioned_contexts[i];
            gchar *uuid_str;

            uuid_str = mbim_uuid_get_printable (&el->context_type);
            mm_dbg ("[%u] context type: %s", el->context_id, mbim_context_type_get_string (mbim_uuid_to_context_type (&el->context_type)));
            mm_dbg ("             uuid: %s", uuid_str);
            mm_dbg ("    access string: %s", el->access_string ? el->access_string : "");
            mm_dbg ("         username: %s", el->user_name ? el->user_name : "");
            mm_dbg ("         password: %s", el->password ? el->password : "");
            mm_dbg ("      compression: %s", mbim_compression_get_string (el->compression));
            mm_dbg ("             auth: %s", mbim_auth_protocol_get_string (el->auth_protocol));
            g_free (uuid_str);
        }

        mbim_provisioned_context_element_array_free (provisioned_contexts);
    } else {
        mm_dbg ("Error listing provisioned contexts: %s", error->message);
        g_error_free (error);
    }

    /* Keep on */
    ctx->step++;
    connect_context_step (ctx);
}

static void
connect_context_step (ConnectContext *ctx)
{
    MbimMessage *message;

    /* If cancelled, complete */
    if (g_cancellable_is_cancelled (ctx->cancellable)) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_CANCELLED,
                                         "Connection setup operation has been cancelled");
        connect_context_complete_and_free (ctx);
        return;
    }

    switch (ctx->step) {
    case CONNECT_STEP_FIRST:
        /* Fall down */
        ctx->step++;

    case CONNECT_STEP_PROVISIONED_CONTEXTS:
        mm_dbg ("Listing provisioned contexts...");
        message = mbim_message_provisioned_contexts_query_new (NULL);
        mbim_device_command (ctx->device,
                             message,
                             10,
                             NULL,
                             (GAsyncReadyCallback)provisioned_contexts_query_ready,
                             ctx);
        mbim_message_unref (message);
        return;

    case CONNECT_STEP_CONNECT: {
        const gchar *apn;
        const gchar *user;
        const gchar *password;
        MbimAuthProtocol auth;
        MMBearerAllowedAuth bearer_auth;
        MbimContextIpType ip_type;
        GError *error = NULL;

        /* Setup parameters to use */

        apn = mm_bearer_properties_get_apn (ctx->properties);
        user = mm_bearer_properties_get_user (ctx->properties);
        password = mm_bearer_properties_get_password (ctx->properties);

        bearer_auth = mm_bearer_properties_get_allowed_auth (ctx->properties);
        if (bearer_auth == MM_BEARER_ALLOWED_AUTH_UNKNOWN) {
            mm_dbg ("Using default (PAP) authentication method");
            auth = MBIM_AUTH_PROTOCOL_PAP;
        } else if (bearer_auth & MM_BEARER_ALLOWED_AUTH_PAP) {
            auth = MBIM_AUTH_PROTOCOL_PAP;
        } else if (bearer_auth & MM_BEARER_ALLOWED_AUTH_CHAP) {
            auth = MBIM_AUTH_PROTOCOL_CHAP;
        } else if (bearer_auth & MM_BEARER_ALLOWED_AUTH_MSCHAPV2) {
            auth = MBIM_AUTH_PROTOCOL_MSCHAPV2;
        } else if (bearer_auth & MM_BEARER_ALLOWED_AUTH_NONE) {
            auth = MBIM_AUTH_PROTOCOL_NONE;
        } else {
            gchar *str;

            str = mm_bearer_allowed_auth_build_string_from_mask (bearer_auth);
            g_simple_async_result_set_error (
                ctx->result,
                MM_CORE_ERROR,
                MM_CORE_ERROR_UNSUPPORTED,
                "Cannot use any of the specified authentication methods (%s)",
                str);
            g_free (str);
            connect_context_complete_and_free (ctx);
            return;
        }

        switch (mm_bearer_properties_get_ip_type (ctx->properties)) {
        case MM_BEARER_IP_FAMILY_IPV4:
            ip_type = MBIM_CONTEXT_IP_TYPE_IPV4;
            break;
        case MM_BEARER_IP_FAMILY_IPV6:
            ip_type = MBIM_CONTEXT_IP_TYPE_IPV6;
            break;
        case MM_BEARER_IP_FAMILY_IPV4V6:
            ip_type = MBIM_CONTEXT_IP_TYPE_IPV4V6;
            break;
        case MM_BEARER_IP_FAMILY_UNKNOWN:
        default:
            mm_dbg ("No specific IP family requested, defaulting to IPv4");
            ip_type = MBIM_CONTEXT_IP_TYPE_IPV4;
            break;
        }

        mm_dbg ("Launching connection with APN '%s'...", apn);
        message = (mbim_message_connect_set_new (
                       ctx->self->priv->session_id,
                       MBIM_ACTIVATION_COMMAND_ACTIVATE,
                       apn ? apn : "",
                       user ? user : "",
                       password ? password : "",
                       MBIM_COMPRESSION_NONE,
                       auth,
                       ip_type,
                       mbim_uuid_from_context_type (MBIM_CONTEXT_TYPE_INTERNET),
                       &error));
        if (!message) {
            g_simple_async_result_take_error (ctx->result, error);
            connect_context_complete_and_free (ctx);
            return;
        }

        mbim_device_command (ctx->device,
                             message,
                             10,
                             NULL,
                             (GAsyncReadyCallback)connect_set_ready,
                             ctx);
        mbim_message_unref (message);
        return;
    }

    case CONNECT_STEP_LAST:
        /* Port is connected; update the state */
        mm_port_set_connected (MM_PORT (ctx->data), TRUE);

        /* Keep the data port */
        g_assert (ctx->self->priv->data == NULL);
        ctx->self->priv->data = g_object_ref (ctx->data);

        /* Set operation result */
        g_simple_async_result_set_op_res_gpointer (
            ctx->result,
            mm_bearer_connect_result_ref (ctx->connect_result),
            (GDestroyNotify)mm_bearer_connect_result_unref);
        connect_context_complete_and_free (ctx);
        return;
    }

    g_assert_not_reached ();
}

static void
_connect (MMBearer *self,
          GCancellable *cancellable,
          GAsyncReadyCallback callback,
          gpointer user_data)
{
    ConnectContext *ctx;
    MMPort *data;
    MbimDevice *device;

    if (!peek_ports (self, &device, &data, callback, user_data))
        return;

    mm_dbg ("Launching connection with data port (%s/%s)",
            mm_port_subsys_get_string (mm_port_get_subsys (data)),
            mm_port_get_device (data));

    ctx = g_slice_new0 (ConnectContext);
    ctx->self = g_object_ref (self);
    ctx->device = g_object_ref (device);;
    ctx->data = g_object_ref (data);
    ctx->cancellable = g_object_ref (cancellable);
    ctx->step = CONNECT_STEP_FIRST;
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             _connect);

    g_object_get (self,
                  MM_BEARER_CONFIG, &ctx->properties,
                  NULL);

    /* Run! */
    connect_context_step (ctx);
}

/*****************************************************************************/

guint32
mm_bearer_mbim_get_session_id (MMBearerMbim *self)
{
    g_return_val_if_fail (MM_IS_BEARER_MBIM (self), 0);

    return self->priv->session_id;
}

/*****************************************************************************/

MMBearer *
mm_bearer_mbim_new (MMBroadbandModemMbim *modem,
                    MMBearerProperties *config,
                    guint32 session_id)
{
    MMBearer *bearer;

    /* The Mbim bearer inherits from MMBearer (so it's not a MMBroadbandBearer)
     * and that means that the object is not async-initable, so we just use
     * g_object_new() here */
    bearer = g_object_new (MM_TYPE_BEARER_MBIM,
                           MM_BEARER_MODEM,           modem,
                           MM_BEARER_CONFIG,          config,
                           MM_BEARER_MBIM_SESSION_ID, (guint)session_id,
                           NULL);

    /* Only export valid bearers */
    mm_bearer_export (bearer);

    return bearer;
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
    MMBearerMbim *self = MM_BEARER_MBIM (object);

    switch (prop_id) {
    case PROP_SESSION_ID:
        self->priv->session_id = g_value_get_uint (value);
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
    MMBearerMbim *self = MM_BEARER_MBIM (object);

    switch (prop_id) {
    case PROP_SESSION_ID:
        g_value_set_uint (value, self->priv->session_id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
mm_bearer_mbim_init (MMBearerMbim *self)
{
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self),
                                              MM_TYPE_BEARER_MBIM,
                                              MMBearerMbimPrivate);
}

static void
dispose (GObject *object)
{
    G_OBJECT_CLASS (mm_bearer_mbim_parent_class)->dispose (object);
}

static void
mm_bearer_mbim_class_init (MMBearerMbimClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBearerClass *bearer_class = MM_BEARER_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBearerMbimPrivate));

    /* Virtual methods */
    object_class->dispose = dispose;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    bearer_class->connect = _connect;
    bearer_class->connect_finish = connect_finish;

    properties[PROP_SESSION_ID] =
        g_param_spec_uint (MM_BEARER_MBIM_SESSION_ID,
                           "Session ID",
                           "Session ID to use with this bearer",
                           0,
                           255,
                           0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_property (object_class, PROP_SESSION_ID, properties[PROP_SESSION_ID]);
}
