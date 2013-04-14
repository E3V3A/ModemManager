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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "mm-modem-helpers-mbim.h"
#include "mm-broadband-modem-mbim.h"
#include "mm-bearer-mbim.h"
#include "mm-sim-mbim.h"

#include "ModemManager.h"
#include "mm-log.h"
#include "mm-errors-types.h"
#include "mm-error-helpers.h"
#include "mm-modem-helpers.h"
#include "mm-bearer-list.h"
#include "mm-iface-modem.h"
#include "mm-iface-modem-3gpp.h"

static void iface_modem_init (MMIfaceModem *iface);
static void iface_modem_3gpp_init (MMIfaceModem3gpp *iface);

G_DEFINE_TYPE_EXTENDED (MMBroadbandModemMbim, mm_broadband_modem_mbim, MM_TYPE_BROADBAND_MODEM, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM, iface_modem_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_3GPP, iface_modem_3gpp_init))

typedef enum {
    PROCESS_NOTIFICATION_FLAG_NONE                 = 0,
    PROCESS_NOTIFICATION_FLAG_SIGNAL_QUALITY       = 1 << 0,
    PROCESS_NOTIFICATION_FLAG_REGISTRATION_UPDATES = 1 << 1,
} ProcessNotificationFlag;

struct _MMBroadbandModemMbimPrivate {
    /* Queried and cached capabilities */
    MbimCellularClass caps_cellular_class;
    MbimDataClass caps_data_class;
    MbimSmsCaps caps_sms;
    guint caps_max_sessions;
    gchar *caps_device_id;
    gchar *caps_firmware_info;

    /* Process unsolicited notifications */
    guint notification_id;
    ProcessNotificationFlag notification_flags;

    /* 3GPP registration helpers */
    gchar *current_operator_id;
    gchar *current_operator_name;
};

/*****************************************************************************/

static gboolean
peek_device (gpointer self,
             MbimDevice **o_device,
             GAsyncReadyCallback callback,
             gpointer user_data)
{
    MMMbimPort *port;

    port = mm_base_modem_peek_port_mbim (MM_BASE_MODEM (self));
    if (!port) {
        g_simple_async_report_error_in_idle (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Couldn't peek MBIM port");
        return FALSE;
    }

    *o_device = mm_mbim_port_peek_device (port);
    return TRUE;
}

/*****************************************************************************/
/* Current Capabilities loading (Modem interface) */

typedef struct {
    MMBroadbandModemMbim *self;
    GSimpleAsyncResult *result;
} LoadCapabilitiesContext;

static void
load_capabilities_context_complete_and_free (LoadCapabilitiesContext *ctx)
{
    g_simple_async_result_complete (ctx->result);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (LoadCapabilitiesContext, ctx);
}

static MMModemCapability
modem_load_current_capabilities_finish (MMIfaceModem *self,
                                        GAsyncResult *res,
                                        GError **error)
{
    MMModemCapability caps;
    gchar *caps_str;

    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return MM_MODEM_CAPABILITY_NONE;

    caps = ((MMModemCapability) GPOINTER_TO_UINT (
                g_simple_async_result_get_op_res_gpointer (
                    G_SIMPLE_ASYNC_RESULT (res))));
    caps_str = mm_modem_capability_build_string_from_mask (caps);
    mm_dbg ("loaded modem capabilities: %s", caps_str);
    g_free (caps_str);
    return caps;
}

static void
device_caps_query_ready (MbimDevice *device,
                         GAsyncResult *res,
                         LoadCapabilitiesContext *ctx)
{
    MMModemCapability mask;
    MbimMessage *response;
    GError *error = NULL;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_device_caps_response_parse (
            response,
            NULL, /* device_type */
            &ctx->self->priv->caps_cellular_class,
            NULL, /* voice_class */
            NULL, /* sim_class */
            &ctx->self->priv->caps_data_class,
            &ctx->self->priv->caps_sms,
            NULL, /* ctrl_caps */
            &ctx->self->priv->caps_max_sessions,
            NULL, /* custom_data_class */
            &ctx->self->priv->caps_device_id,
            &ctx->self->priv->caps_firmware_info,
            NULL, /* hardware_info */
            &error)) {
        /* Build mask of modem capabilities */
        mask = 0;
        if (ctx->self->priv->caps_cellular_class & MBIM_CELLULAR_CLASS_GSM)
            mask |= MM_MODEM_CAPABILITY_GSM_UMTS;
        if (ctx->self->priv->caps_cellular_class & MBIM_CELLULAR_CLASS_CDMA)
            mask |= MM_MODEM_CAPABILITY_CDMA_EVDO;
        if (ctx->self->priv->caps_data_class & MBIM_DATA_CLASS_LTE)
            mask |= MM_MODEM_CAPABILITY_LTE;
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   GUINT_TO_POINTER (mask),
                                                   NULL);
    } else
        g_simple_async_result_take_error (ctx->result, error);

    if (response)
        mbim_message_unref (response);
    load_capabilities_context_complete_and_free (ctx);
}

static void
modem_load_current_capabilities (MMIfaceModem *self,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    LoadCapabilitiesContext *ctx;
    MbimDevice *device;
    MbimMessage *message;

    if (!peek_device (self, &device, callback, user_data))
        return;

    ctx = g_slice_new (LoadCapabilitiesContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             modem_load_current_capabilities);

    mm_dbg ("loading current capabilities...");
    message = mbim_message_device_caps_query_new (NULL);
    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         (GAsyncReadyCallback)device_caps_query_ready,
                         ctx);
    mbim_message_unref (message);
}

/*****************************************************************************/
/* Manufacturer loading (Modem interface) */

static gchar *
modem_load_manufacturer_finish (MMIfaceModem *self,
                                GAsyncResult *res,
                                GError **error)
{
    return g_strdup (mm_base_modem_get_plugin (MM_BASE_MODEM (self)));
}

static void
modem_load_manufacturer (MMIfaceModem *self,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
    GSimpleAsyncResult *result;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_manufacturer);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Model loading (Modem interface) */

static gchar *
modem_load_model_finish (MMIfaceModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    return g_strdup_printf ("MBIM [%04X:%04X]",
                            (mm_base_modem_get_vendor_id (MM_BASE_MODEM (self)) & 0xFFFF),
                            (mm_base_modem_get_product_id (MM_BASE_MODEM (self)) & 0xFFFF));

}

static void
modem_load_model (MMIfaceModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    GSimpleAsyncResult *result;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_manufacturer);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Revision loading (Modem interface) */

static gchar *
modem_load_revision_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    if (MM_BROADBAND_MODEM_MBIM (self)->priv->caps_firmware_info)
        return g_strdup (MM_BROADBAND_MODEM_MBIM (self)->priv->caps_firmware_info);

    g_set_error (error,
                 MM_CORE_ERROR,
                 MM_CORE_ERROR_FAILED,
                 "Firmware revision information not given in device capabilities");
    return NULL;
}

static void
modem_load_revision (MMIfaceModem *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    GSimpleAsyncResult *result;

    /* Just complete */
    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_revision);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Equipment Identifier loading (Modem interface) */

static gchar *
modem_load_equipment_identifier_finish (MMIfaceModem *self,
                                        GAsyncResult *res,
                                        GError **error)
{
    if (MM_BROADBAND_MODEM_MBIM (self)->priv->caps_device_id)
        return g_strdup (MM_BROADBAND_MODEM_MBIM (self)->priv->caps_device_id);

    g_set_error (error,
                 MM_CORE_ERROR,
                 MM_CORE_ERROR_FAILED,
                 "Device ID not given in device capabilities");
    return NULL;
}

static void
modem_load_equipment_identifier (MMIfaceModem *self,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
    GSimpleAsyncResult *result;

    /* Just complete */
    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_equipment_identifier);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Device identifier loading (Modem interface) */

static gchar *
modem_load_device_identifier_finish (MMIfaceModem *self,
                                     GAsyncResult *res,
                                     GError **error)
{
    gchar *device_identifier;

    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    device_identifier = g_strdup (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
    return device_identifier;
}

static void
modem_load_device_identifier (MMIfaceModem *self,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
    GSimpleAsyncResult *result;
    gchar *device_identifier;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_device_identifier);

    /* Just use dummy ATI/ATI1 replies, all the other internal info should be
     * enough for uniqueness */
    device_identifier = mm_broadband_modem_create_device_identifier (MM_BROADBAND_MODEM (self), "", "");
    g_simple_async_result_set_op_res_gpointer (result,
                                               device_identifier,
                                               (GDestroyNotify)g_free);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Supported modes loading (Modem interface) */

static MMModemMode
modem_load_supported_modes_finish (MMIfaceModem *_self,
                                   GAsyncResult *res,
                                   GError **error)
{
    MMModemMode mask;
    MMBroadbandModemMbim *self = MM_BROADBAND_MODEM_MBIM (_self);

    if (self->priv->caps_data_class == 0) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Data class not given in device capabilities");
        return MM_MODEM_MODE_NONE;
    }

    mask = 0;

    /* 3GPP... */
    if (self->priv->caps_data_class & (MBIM_DATA_CLASS_GPRS |
                                       MBIM_DATA_CLASS_EDGE))
        mask |= MM_MODEM_MODE_2G;
    if (self->priv->caps_data_class & (MBIM_DATA_CLASS_UMTS |
                                       MBIM_DATA_CLASS_HSDPA |
                                       MBIM_DATA_CLASS_HSUPA))
        mask |= MM_MODEM_MODE_3G;
    if (self->priv->caps_data_class & MBIM_DATA_CLASS_LTE)
        mask |= MM_MODEM_MODE_4G;

    /* 3GPP2... */
    if (self->priv->caps_data_class & MBIM_DATA_CLASS_1XRTT)
        mask |= MM_MODEM_MODE_2G;
    if (self->priv->caps_data_class & (MBIM_DATA_CLASS_1XEVDO |
                                       MBIM_DATA_CLASS_1XEVDO_REVA |
                                       MBIM_DATA_CLASS_1XEVDV |
                                       MBIM_DATA_CLASS_3XRTT |
                                       MBIM_DATA_CLASS_1XEVDO_REVB))
        mask |= MM_MODEM_MODE_3G;
    if (self->priv->caps_data_class & MBIM_DATA_CLASS_UMB)
        mask |= MM_MODEM_MODE_4G;

    return mask;
}

static void
modem_load_supported_modes (MMIfaceModem *self,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    GSimpleAsyncResult *result;

    /* Just complete */
    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_supported_modes);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Unlock required loading (Modem interface) */

typedef struct {
    MMBroadbandModemMbim *self;
    GSimpleAsyncResult *result;
    guint n_ready_status_checks;
    MbimDevice *device;
} LoadUnlockRequiredContext;

static void
load_unlock_required_context_complete_and_free (LoadUnlockRequiredContext *ctx)
{
    g_simple_async_result_complete (ctx->result);
    g_object_unref (ctx->result);
    g_object_unref (ctx->device);
    g_object_unref (ctx->self);
    g_slice_free (LoadUnlockRequiredContext, ctx);
}

static MMModemLock
modem_load_unlock_required_finish (MMIfaceModem *self,
                                   GAsyncResult *res,
                                   GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return MM_MODEM_LOCK_UNKNOWN;

    return (MMModemLock) GPOINTER_TO_UINT (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

static void
pin_query_ready (MbimDevice *device,
                 GAsyncResult *res,
                 LoadUnlockRequiredContext *ctx)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimPinType pin_type;
    MbimPinState pin_state;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_pin_response_parse (
            response,
            &pin_type,
            &pin_state,
            NULL,
            &error)) {
        MMModemLock unlock_required;

        if (pin_state == MBIM_PIN_STATE_UNLOCKED)
            unlock_required = MM_MODEM_LOCK_NONE;
        else
            unlock_required = mm_modem_lock_from_mbim_pin_type (pin_type);
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   GUINT_TO_POINTER (unlock_required),
                                                   NULL);
    } else
        g_simple_async_result_take_error (ctx->result, error);

    if (response)
        mbim_message_unref (response);
    load_unlock_required_context_complete_and_free (ctx);
}

static gboolean wait_for_sim_ready (LoadUnlockRequiredContext *ctx);

static void
unlock_required_subscriber_ready_state_ready (MbimDevice *device,
                                              GAsyncResult *res,
                                              LoadUnlockRequiredContext *ctx)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimSubscriberReadyState ready_state = MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_subscriber_ready_status_response_parse (
            response,
            &ready_state,
            NULL, /* subscriber_id */
            NULL, /* sim_iccid */
            NULL, /* ready_info */
            NULL, /* telephone_numbers_count */
            NULL, /* telephone_numbers */
            &error)) {
        switch (ready_state) {
        case MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED:
        case MBIM_SUBSCRIBER_READY_STATE_INITIALIZED:
        case MBIM_SUBSCRIBER_READY_STATE_DEVICE_LOCKED:
            /* Don't set error */
            break;
        case MBIM_SUBSCRIBER_READY_STATE_SIM_NOT_INSERTED:
            error = mm_mobile_equipment_error_for_code (MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED);
            break;
        case MBIM_SUBSCRIBER_READY_STATE_BAD_SIM:
            error = mm_mobile_equipment_error_for_code (MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG);
            break;
        case MBIM_SUBSCRIBER_READY_STATE_FAILURE:
        case MBIM_SUBSCRIBER_READY_STATE_NOT_ACTIVATED:
        default:
            error = mm_mobile_equipment_error_for_code (MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE);
            break;
        }
    }

    /* Fatal errors are reported right away */
    if (error) {
        g_simple_async_result_take_error (ctx->result, error);
        load_unlock_required_context_complete_and_free (ctx);
    }
    /* Need to retry? */
    else if (ready_state == MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED) {
        if (--ctx->n_ready_status_checks == 0) {
            g_simple_async_result_set_error (ctx->result,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Error waiting for SIM to get initialized");
            load_unlock_required_context_complete_and_free (ctx);
        } else {
            /* Retry */
            g_timeout_add_seconds (1, (GSourceFunc)wait_for_sim_ready, ctx);
        }
    }
    /* Initialized but locked? */
    else if (ready_state == MBIM_SUBSCRIBER_READY_STATE_DEVICE_LOCKED) {
        MbimMessage *message;

        /* Query which lock is to unlock */
        message = mbim_message_pin_query_new (NULL);
        mbim_device_command (device,
                             message,
                             10,
                             NULL,
                             (GAsyncReadyCallback)pin_query_ready,
                             ctx);
        mbim_message_unref (message);
    }
    /* Initialized but locked? */
    else if (ready_state == MBIM_SUBSCRIBER_READY_STATE_INITIALIZED) {
        g_simple_async_result_set_op_res_gpointer (ctx->result,
                                                   GUINT_TO_POINTER (MM_MODEM_LOCK_NONE),
                                                   NULL);
        load_unlock_required_context_complete_and_free (ctx);
    } else
        g_assert_not_reached ();

    if (response)
        mbim_message_unref (response);
}

static gboolean
wait_for_sim_ready (LoadUnlockRequiredContext *ctx)
{
    MbimMessage *message;

    message = mbim_message_subscriber_ready_status_query_new (NULL);
    mbim_device_command (ctx->device,
                         message,
                         10,
                         NULL,
                         (GAsyncReadyCallback)unlock_required_subscriber_ready_state_ready,
                         ctx);
    mbim_message_unref (message);
    return FALSE;
}

static void
modem_load_unlock_required (MMIfaceModem *self,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    LoadUnlockRequiredContext *ctx;
    MbimDevice *device;

    if (!peek_device (self, &device, callback, user_data))
        return;

    ctx = g_slice_new (LoadUnlockRequiredContext);
    ctx->self = g_object_ref (self);
    ctx->device = g_object_ref (device);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_unlock_required);
    ctx->n_ready_status_checks = 10;

    wait_for_sim_ready (ctx);
}

/*****************************************************************************/
/* Unlock retries loading (Modem interface) */

static MMUnlockRetries *
modem_load_unlock_retries_finish (MMIfaceModem *self,
                                  GAsyncResult *res,
                                  GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return MM_UNLOCK_RETRIES (g_object_ref (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res))));
}

static void
pin_query_unlock_retries_ready (MbimDevice *device,
                                GAsyncResult *res,
                                GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimPinType pin_type;
    guint32 remaining_attempts;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_pin_response_parse (
            response,
            &pin_type,
            NULL,
            &remaining_attempts,
            &error)) {
        MMUnlockRetries *retries;

        retries = mm_unlock_retries_new ();
        mm_unlock_retries_set (retries,
                               mm_modem_lock_from_mbim_pin_type (pin_type),
                               remaining_attempts);
        g_simple_async_result_set_op_res_gpointer (simple, retries, g_object_unref);
    } else
        g_simple_async_result_take_error (simple, error);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
modem_load_unlock_retries (MMIfaceModem *self,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    GSimpleAsyncResult *result;
    MbimDevice *device;
    MbimMessage *message;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_unlock_retries);

    message = mbim_message_pin_query_new (NULL);
    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         (GAsyncReadyCallback)pin_query_unlock_retries_ready,
                         result);
    mbim_message_unref (message);
}

/*****************************************************************************/
/* Own numbers loading */

static GStrv
modem_load_own_numbers_finish (MMIfaceModem *self,
                               GAsyncResult *res,
                               GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
}

static void
own_numbers_subscriber_ready_state_ready (MbimDevice *device,
                                          GAsyncResult *res,
                                          GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    gchar **telephone_numbers;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_subscriber_ready_status_response_parse (
            response,
            NULL, /* ready_state */
            NULL, /* subscriber_id */
            NULL, /* sim_iccid */
            NULL, /* ready_info */
            NULL, /* telephone_numbers_count */
            &telephone_numbers,
            &error)) {
        g_simple_async_result_set_op_res_gpointer (simple, telephone_numbers, NULL);
    } else
        g_simple_async_result_take_error (simple, error);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
modem_load_own_numbers (MMIfaceModem *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    GSimpleAsyncResult *result;
    MbimDevice *device;
    MbimMessage *message;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_own_numbers);

    message = mbim_message_subscriber_ready_status_query_new (NULL);
    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         (GAsyncReadyCallback)own_numbers_subscriber_ready_state_ready,
                         result);
    mbim_message_unref (message);
}

/*****************************************************************************/
/* Initial power state loading */

static MMModemPowerState
modem_load_power_state_finish (MMIfaceModem *self,
                               GAsyncResult *res,
                               GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return MM_MODEM_POWER_STATE_UNKNOWN;

    return (MMModemPowerState) GPOINTER_TO_UINT (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

static void
radio_state_query_ready (MbimDevice *device,
                         GAsyncResult *res,
                         GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimRadioSwitchState hardware_radio_state;
    MbimRadioSwitchState software_radio_state;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_radio_state_response_parse (
            response,
            &hardware_radio_state,
            &software_radio_state,
            &error)) {
        MMModemPowerState state;

        if (hardware_radio_state == MBIM_RADIO_SWITCH_STATE_OFF ||
            software_radio_state == MBIM_RADIO_SWITCH_STATE_OFF)
            state = MM_MODEM_POWER_STATE_LOW;
        else
            state = MM_MODEM_POWER_STATE_ON;
        g_simple_async_result_set_op_res_gpointer (simple,
                                                   GUINT_TO_POINTER (state),
                                                   NULL);
    } else
        g_simple_async_result_take_error (simple, error);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
modem_load_power_state (MMIfaceModem *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    GSimpleAsyncResult *result;
    MbimDevice *device;
    MbimMessage *message;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_power_state);

    message = mbim_message_radio_state_query_new (NULL);
    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         (GAsyncReadyCallback)radio_state_query_ready,
                         result);
    mbim_message_unref (message);
}

/*****************************************************************************/
/* Power up/down (Modem interface) */

static gboolean
common_power_up_down_finish (MMIfaceModem *self,
                             GAsyncResult *res,
                             GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
radio_state_set_down_ready (MbimDevice *device,
                            GAsyncResult *res,
                            GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;

    response = mbim_device_command_finish (device, res, &error);
    if (response)
        mbim_message_command_done_get_result (response, &error);

    if (error)
        g_simple_async_result_take_error (simple, error);
    else
        g_simple_async_result_set_op_res_gboolean (simple, TRUE);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
radio_state_set_up_ready (MbimDevice *device,
                          GAsyncResult *res,
                          GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimRadioSwitchState hardware_radio_state;
    MbimRadioSwitchState software_radio_state;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_radio_state_response_parse (
            response,
            &hardware_radio_state,
            &software_radio_state,
            &error)) {
        if (hardware_radio_state == MBIM_RADIO_SWITCH_STATE_OFF)
            error = g_error_new (MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Cannot power-up: hardware radio switch is OFF");
        else if (software_radio_state == MBIM_RADIO_SWITCH_STATE_OFF)
            g_warn_if_reached ();
    }

    if (error)
        g_simple_async_result_take_error (simple, error);
    else
        g_simple_async_result_set_op_res_gboolean (simple, TRUE);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
common_power_up_down (MMIfaceModem *self,
                      gboolean up,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
    GSimpleAsyncResult *result;
    MbimDevice *device;
    MbimMessage *message;
    MbimRadioSwitchState state;
    GAsyncReadyCallback ready_cb;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        common_power_up_down);

    if (up) {
        ready_cb = (GAsyncReadyCallback)radio_state_set_up_ready;
        state = MBIM_RADIO_SWITCH_STATE_ON;
    } else {
        ready_cb = (GAsyncReadyCallback)radio_state_set_down_ready;
        state = MBIM_RADIO_SWITCH_STATE_OFF;
    }

    message = mbim_message_radio_state_set_new (state, NULL);
    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         ready_cb,
                         result);
    mbim_message_unref (message);
}

static void
modem_power_down (MMIfaceModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    common_power_up_down (self, FALSE, callback, user_data);
}

static void
modem_power_up (MMIfaceModem *self,
                GAsyncReadyCallback callback,
                gpointer user_data)
{
    common_power_up_down (self, TRUE, callback, user_data);
}

/*****************************************************************************/
/* Create Bearer (Modem interface) */

static MMBearer *
modem_create_bearer_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    MMBearer *bearer;

    bearer = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
    mm_dbg ("New bearer created at DBus path '%s'", mm_bearer_get_path (bearer));

    return g_object_ref (bearer);
}

typedef struct {
    guint32 session_id;
    gboolean found;
} FindSessionId;

static void
bearer_list_session_id_foreach (MMBearer *bearer,
                                gpointer user_data)
{
    FindSessionId *ctx = user_data;

    if (!ctx->found &&
        MM_IS_BEARER_MBIM (bearer) &&
        mm_bearer_mbim_get_session_id (MM_BEARER_MBIM (bearer)) == ctx->session_id)
        ctx->found = TRUE;
}

static guint32
find_next_bearer_session_id (MMBroadbandModemMbim *self)
{
    MMBearerList *bearer_list;
    guint i;

    g_object_get (self,
                  MM_IFACE_MODEM_BEARER_LIST, &bearer_list,
                  NULL);

    if (!bearer_list)
        return 1;

    for (i = 1; i <= 255; i++) {
        FindSessionId ctx;

        ctx.session_id = i;
        ctx.found = FALSE;

        mm_bearer_list_foreach (bearer_list,
                                bearer_list_session_id_foreach,
                                &ctx);

        if (!ctx.found) {
            g_object_unref (bearer_list);
            return i;
        }
    }

    /* no valid session id found */
    g_object_unref (bearer_list);
    return 0;
}

static void
modem_create_bearer (MMIfaceModem *self,
                     MMBearerProperties *properties,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    MMBearer *bearer;
    GSimpleAsyncResult *result;
    guint32 session_id;

    /* Set a new ref to the bearer object as result */
    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_create_bearer);

    /* Find a new session ID */
    session_id = find_next_bearer_session_id (MM_BROADBAND_MODEM_MBIM (self));
    if (!session_id) {
        g_simple_async_result_set_error (
            result,
            MM_CORE_ERROR,
            MM_CORE_ERROR_FAILED,
            "Not enough session IDs");
        g_simple_async_result_complete_in_idle (result);
        g_object_unref (result);
        return;
    }

    /* We just create a MMBearerMbim */
    mm_dbg ("Creating MBIM bearer in MBIM modem");
    bearer = mm_bearer_mbim_new (MM_BROADBAND_MODEM_MBIM (self),
                                 properties,
                                 session_id);

    g_simple_async_result_set_op_res_gpointer (result, bearer, g_object_unref);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Create SIM (Modem interface) */

static MMSim *
create_sim_finish (MMIfaceModem *self,
                   GAsyncResult *res,
                   GError **error)
{
    return mm_sim_mbim_new_finish (res, error);
}

static void
create_sim (MMIfaceModem *self,
            GAsyncReadyCallback callback,
            gpointer user_data)
{
    /* New MBIM SIM */
    mm_sim_mbim_new (MM_BASE_MODEM (self),
                    NULL, /* cancellable */
                    callback,
                    user_data);
}

/*****************************************************************************/
/* First enabling step */

static gboolean
enabling_started_finish (MMBroadbandModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
parent_enabling_started_ready (MMBroadbandModem *self,
                               GAsyncResult *res,
                               GSimpleAsyncResult *simple)
{
    GError *error = NULL;

    if (!MM_BROADBAND_MODEM_CLASS (mm_broadband_modem_mbim_parent_class)->enabling_started_finish (
            self,
            res,
            &error)) {
        /* Don't treat this as fatal. Parent enabling may fail if it cannot grab a primary
         * AT port, which isn't really an issue in MBIM-based modems */
        mm_dbg ("Couldn't start parent enabling: %s", error->message);
        g_error_free (error);
    }

    g_simple_async_result_set_op_res_gboolean (simple, TRUE);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
enabling_started (MMBroadbandModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    GSimpleAsyncResult *result;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        enabling_started);
    MM_BROADBAND_MODEM_CLASS (mm_broadband_modem_mbim_parent_class)->enabling_started (
        self,
        (GAsyncReadyCallback)parent_enabling_started_ready,
        result);
}

/*****************************************************************************/
/* First initialization step */

typedef struct {
    MMBroadbandModem *self;
    GSimpleAsyncResult *result;
    MMMbimPort *mbim;
} InitializationStartedContext;

static void
initialization_started_context_complete_and_free (InitializationStartedContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    if (ctx->mbim)
        g_object_unref (ctx->mbim);
    g_object_unref (ctx->result);
    g_object_unref (ctx->self);
    g_slice_free (InitializationStartedContext, ctx);
}

static gpointer
initialization_started_finish (MMBroadbandModem *self,
                               GAsyncResult *res,
                               GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    /* Just parent's pointer passed here */
    return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));
}

static void
parent_initialization_started_ready (MMBroadbandModem *self,
                                     GAsyncResult *res,
                                     InitializationStartedContext *ctx)
{
    gpointer parent_ctx;
    GError *error = NULL;

    parent_ctx = MM_BROADBAND_MODEM_CLASS (mm_broadband_modem_mbim_parent_class)->initialization_started_finish (
        self,
        res,
        &error);
    if (error) {
        /* Don't treat this as fatal. Parent initialization may fail if it cannot grab a primary
         * AT port, which isn't really an issue in MBIM-based modems */
        mm_dbg ("Couldn't start parent initialization: %s", error->message);
        g_error_free (error);
    }

    g_simple_async_result_set_op_res_gpointer (ctx->result, parent_ctx, NULL);
    initialization_started_context_complete_and_free (ctx);
}

static void
parent_initialization_started (InitializationStartedContext *ctx)
{
    MM_BROADBAND_MODEM_CLASS (mm_broadband_modem_mbim_parent_class)->initialization_started (
        ctx->self,
        (GAsyncReadyCallback)parent_initialization_started_ready,
        ctx);
}

static void
mbim_port_open_ready (MMMbimPort *mbim,
                      GAsyncResult *res,
                      InitializationStartedContext *ctx)
{
    GError *error = NULL;

    if (!mm_mbim_port_open_finish (mbim, res, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        initialization_started_context_complete_and_free (ctx);
        return;
    }

    /* Done we are, launch parent's callback */
    parent_initialization_started (ctx);
}

static void
initialization_started (MMBroadbandModem *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    InitializationStartedContext *ctx;

    ctx = g_slice_new0 (InitializationStartedContext);
    ctx->self = g_object_ref (self);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             initialization_started);
    ctx->mbim = mm_base_modem_get_port_mbim (MM_BASE_MODEM (self));

    /* This may happen if we unplug the modem unexpectedly */
    if (!ctx->mbim) {
        g_simple_async_result_set_error (ctx->result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "Cannot initialize: MBIM port went missing");
        initialization_started_context_complete_and_free (ctx);
        return;
    }

    if (mm_mbim_port_is_open (ctx->mbim)) {
        /* Nothing to be done, just launch parent's callback */
        parent_initialization_started (ctx);
        return;
    }

    /* Now open our MBIM port */
    mm_mbim_port_open (ctx->mbim,
                       NULL,
                       (GAsyncReadyCallback)mbim_port_open_ready,
                       ctx);
}

/*****************************************************************************/
/* IMEI loading (3GPP interface) */

static gchar *
modem_3gpp_load_imei_finish (MMIfaceModem3gpp *self,
                             GAsyncResult *res,
                             GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return NULL;

    return g_strdup (g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res)));
}

static void
modem_3gpp_load_imei (MMIfaceModem3gpp *_self,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
    MMBroadbandModemMbim *self = MM_BROADBAND_MODEM_MBIM (_self);
    GSimpleAsyncResult *result;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_3gpp_load_imei);

    if (self->priv->caps_device_id)
        g_simple_async_result_set_op_res_gpointer (result,
                                                   self->priv->caps_device_id,
                                                   NULL);
    else
        g_simple_async_result_set_error (result,
                                         MM_CORE_ERROR,
                                         MM_CORE_ERROR_FAILED,
                                         "Device doesn't report a valid IMEI");
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Facility locks status loading (3GPP interface) */

static MMModem3gppFacility
modem_3gpp_load_enabled_facility_locks_finish (MMIfaceModem3gpp *self,
                                               GAsyncResult *res,
                                               GError **error)
{
    if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error))
        return MM_MODEM_3GPP_FACILITY_NONE;

    return ((MMModem3gppFacility) GPOINTER_TO_UINT (
                g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res))));
}

static void
pin_list_query_ready (MbimDevice *device,
                      GAsyncResult *res,
                      GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimPinDesc *pin_desc_pin1;
    MbimPinDesc *pin_desc_pin2;
    MbimPinDesc *pin_desc_device_sim_pin;
    MbimPinDesc *pin_desc_device_first_sim_pin;
    MbimPinDesc *pin_desc_network_pin;
    MbimPinDesc *pin_desc_network_subset_pin;
    MbimPinDesc *pin_desc_service_provider_pin;
    MbimPinDesc *pin_desc_corporate_pin;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_pin_list_response_parse (
            response,
            &pin_desc_pin1,
            &pin_desc_pin2,
            &pin_desc_device_sim_pin,
            &pin_desc_device_first_sim_pin,
            &pin_desc_network_pin,
            &pin_desc_network_subset_pin,
            &pin_desc_service_provider_pin,
            &pin_desc_corporate_pin,
            NULL, /* pin_desc_subsidy_lock */
            NULL, /* pin_desc_custom */
            &error)) {
        MMModem3gppFacility mask = MM_MODEM_3GPP_FACILITY_NONE;

        if (pin_desc_pin1->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_SIM;
        mbim_pin_desc_free (pin_desc_pin1);

        if (pin_desc_pin2->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_FIXED_DIALING;
        mbim_pin_desc_free (pin_desc_pin2);

        if (pin_desc_device_sim_pin->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_PH_SIM;
        mbim_pin_desc_free (pin_desc_device_sim_pin);

        if (pin_desc_device_first_sim_pin->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_PH_FSIM;
        mbim_pin_desc_free (pin_desc_device_first_sim_pin);

        if (pin_desc_network_pin->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_NET_PERS;
        mbim_pin_desc_free (pin_desc_network_pin);

        if (pin_desc_network_subset_pin->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_NET_SUB_PERS;
        mbim_pin_desc_free (pin_desc_network_subset_pin);

        if (pin_desc_service_provider_pin->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_PROVIDER_PERS;
        mbim_pin_desc_free (pin_desc_service_provider_pin);

        if (pin_desc_corporate_pin->pin_mode == MBIM_PIN_MODE_ENABLED)
            mask |= MM_MODEM_3GPP_FACILITY_CORP_PERS;
        mbim_pin_desc_free (pin_desc_corporate_pin);

        g_simple_async_result_set_op_res_gpointer (simple,
                                                   GUINT_TO_POINTER (mask),
                                                   NULL);
    } else
        g_simple_async_result_take_error (simple, error);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
modem_3gpp_load_enabled_facility_locks (MMIfaceModem3gpp *self,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
    GSimpleAsyncResult *result;
    MbimDevice *device;
    MbimMessage *message;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_load_unlock_retries);

    message = mbim_message_pin_list_query_new (NULL);
    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         (GAsyncReadyCallback)pin_list_query_ready,
                         result);
    mbim_message_unref (message);
}

/*****************************************************************************/
/* Common unsolicited events setup and cleanup */

static void
basic_connect_notification_signal_state (MMBroadbandModemMbim *self,
                                         MbimMessage *notification)
{
    guint32 rssi;

    if (mbim_message_signal_state_notification_parse (
            notification,
            &rssi,
            NULL, /* error_rate */
            NULL, /* signal_strength_interval */
            NULL, /* rssi_threshold */
            NULL, /* error_rate_threshold */
            NULL)) {
        guint32 quality;

        /* Normalize the quality. 99 means unknown, we default it to 0 */
        quality = CLAMP (rssi == 99 ? 0 : rssi, 0, 31) * 100 / 31;

        mm_dbg ("Signal state indication: %u --> %u%%", rssi, quality);
        mm_iface_modem_update_signal_quality (MM_IFACE_MODEM (self), quality);
    }
}

static void
update_registration_info (MMBroadbandModemMbim *self,
                          MbimRegisterState state,
                          MbimDataClass available_data_classes,
                          gchar *operator_id_take,
                          gchar *operator_name_take)
{
    MMModem3gppRegistrationState reg_state;
    MMModemAccessTechnology act;

    reg_state = mm_modem_3gpp_registration_state_from_mbim_register_state (state);
    act = mm_modem_access_technology_from_mbim_data_class (available_data_classes);

    if (reg_state == MM_MODEM_3GPP_REGISTRATION_STATE_HOME ||
        reg_state == MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING) {
        if (self->priv->current_operator_id &&
            g_str_equal (self->priv->current_operator_id, operator_id_take)) {
            g_free (operator_id_take);
        } else {
            g_free (self->priv->current_operator_id);
            self->priv->current_operator_id = operator_id_take;
        }

        if (self->priv->current_operator_name &&
            g_str_equal (self->priv->current_operator_name, operator_name_take)) {
            g_free (operator_name_take);
        } else {
            g_free (self->priv->current_operator_name);
            self->priv->current_operator_name = operator_name_take;
        }
    } else {
        if (self->priv->current_operator_id) {
            g_free (self->priv->current_operator_id);
            self->priv->current_operator_id = 0;
        }
        if (self->priv->current_operator_name) {
            g_free (self->priv->current_operator_name);
            self->priv->current_operator_name = 0;
        }
        g_free (operator_id_take);
        g_free (operator_name_take);
    }

    mm_iface_modem_3gpp_update_ps_registration_state (
        MM_IFACE_MODEM_3GPP (self),
        reg_state);

    mm_iface_modem_3gpp_update_access_technologies (
        MM_IFACE_MODEM_3GPP (self),
        act);
}

static void
basic_connect_notification_register_state (MMBroadbandModemMbim *self,
                                           MbimMessage *notification)
{
    MbimRegisterState register_state;
    MbimDataClass available_data_classes;
    gchar *provider_id;
    gchar *provider_name;

    if (mbim_message_register_state_notification_parse (
            notification,
            NULL, /* nw_error */
            &register_state,
            NULL, /* register_mode */
            &available_data_classes,
            NULL, /* current_cellular_class */
            &provider_id,
            &provider_name,
            NULL, /* roaming_text */
            NULL, /* registration_flag */
            NULL)) {
        update_registration_info (self,
                                  register_state,
                                  available_data_classes,
                                  provider_id,
                                  provider_name);
    }
}

static void
basic_connect_notification (MMBroadbandModemMbim *self,
                            MbimMessage *notification)
{
    switch (mbim_message_indicate_status_get_cid (notification)) {
    case MBIM_CID_BASIC_CONNECT_SIGNAL_STATE:
        if (self->priv->notification_flags & PROCESS_NOTIFICATION_FLAG_SIGNAL_QUALITY)
            basic_connect_notification_signal_state (self, notification);
        break;
    case MBIM_CID_BASIC_CONNECT_REGISTER_STATE:
        if (self->priv->notification_flags & PROCESS_NOTIFICATION_FLAG_REGISTRATION_UPDATES)
            basic_connect_notification_register_state (self, notification);
        break;
    default:
        /* Ignore */
        break;
    }
}

static void
device_notification_cb (MbimDevice *device,
                        MbimMessage *notification,
                        MMBroadbandModemMbim *self)
{
    switch (mbim_message_indicate_status_get_service (notification)) {
    case MBIM_SERVICE_BASIC_CONNECT:
        basic_connect_notification (self, notification);
        break;
    default:
        /* Ignore */
        break;
    }
}

static gboolean
common_setup_cleanup_unsolicited_events_finish (MMIfaceModem3gpp *self,
                                                GAsyncResult *res,
                                                GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
common_setup_cleanup_unsolicited_events (MMIfaceModem3gpp *_self,
                                         gboolean setup,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
    MMBroadbandModemMbim *self = MM_BROADBAND_MODEM_MBIM (_self);
    MbimDevice *device;
    GSimpleAsyncResult *result;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        common_setup_cleanup_unsolicited_events);

    if (setup) {
        /* Don't re-enable it if already there */
        if (!self->priv->notification_id)
            self->priv->notification_id =
                g_signal_connect (device,
                                  MBIM_DEVICE_SIGNAL_INDICATE_STATUS,
                                  G_CALLBACK (device_notification_cb),
                                  self);
    } else {
        /* Don't remove the signal if there are still listeners interested */
        if (self->priv->notification_flags == PROCESS_NOTIFICATION_FLAG_NONE &&
            self->priv->notification_id &&
            g_signal_handler_is_connected (device, self->priv->notification_id))
            g_signal_handler_disconnect (device, self->priv->notification_id);
        self->priv->notification_id = 0;
    }

    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Setup/cleanup unsolicited events */

static void
cleanup_unsolicited_events (MMIfaceModem3gpp *self,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    MM_BROADBAND_MODEM_MBIM (self)->priv->notification_flags &= ~PROCESS_NOTIFICATION_FLAG_SIGNAL_QUALITY;
    common_setup_cleanup_unsolicited_events (self, FALSE, callback, user_data);
}

static void
setup_unsolicited_events (MMIfaceModem3gpp *self,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    MM_BROADBAND_MODEM_MBIM (self)->priv->notification_flags |= PROCESS_NOTIFICATION_FLAG_SIGNAL_QUALITY;
    common_setup_cleanup_unsolicited_events (self, TRUE, callback, user_data);
}

/*****************************************************************************/
/* Cleanup/Setup unsolicited registration events */

static void
cleanup_unsolicited_registration_events (MMIfaceModem3gpp *self,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
    MM_BROADBAND_MODEM_MBIM (self)->priv->notification_flags &= ~PROCESS_NOTIFICATION_FLAG_REGISTRATION_UPDATES;
    common_setup_cleanup_unsolicited_events (self, FALSE, callback, user_data);
}

static void
setup_unsolicited_registration_events (MMIfaceModem3gpp *self,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    MM_BROADBAND_MODEM_MBIM (self)->priv->notification_flags |= PROCESS_NOTIFICATION_FLAG_REGISTRATION_UPDATES;
    common_setup_cleanup_unsolicited_events (self, TRUE, callback, user_data);
}

/*****************************************************************************/
/* Enable/Disable unsolicited events */

static gboolean
common_enable_disable_unsolicited_events_finish (MMIfaceModem3gpp *self,
                                                 GAsyncResult *res,
                                                 GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
disable_signal_state_set_ready_cb (MbimDevice *device,
                                   GAsyncResult *res,
                                   GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;

    response = mbim_device_command_finish (device, res, &error);
    if (response)
        mbim_message_command_done_get_result (response, &error);

    if (error)
        g_simple_async_result_take_error (simple, error);
    else
        g_simple_async_result_set_op_res_gboolean (simple, TRUE);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
enable_signal_state_set_ready_cb (MbimDevice *device,
                                  GAsyncResult *res,
                                  GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    guint32 rssi;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_signal_state_response_parse (
            response,
            &rssi,
            NULL, /* error_rate */
            NULL, /* signal_strength_interval */
            NULL, /* rssi_threshold */
            NULL, /* error_rate_threshold */
            &error)) {
        guint32 quality;
        GObject *self;

        /* Normalize the quality. 99 means unknown, we default it to 0 */
        quality = CLAMP (rssi == 99 ? 0 : rssi, 0, 31) * 100 / 31;
        mm_dbg ("Initial signal state: %u --> %u%%", rssi, quality);

        self = g_async_result_get_source_object (G_ASYNC_RESULT (simple));
        mm_iface_modem_update_signal_quality (MM_IFACE_MODEM (self), quality);
        g_object_unref (self);

        g_simple_async_result_set_op_res_gboolean (simple, TRUE);
    } else
        g_simple_async_result_take_error (simple, error);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
common_enable_disable_unsolicited_events (MMIfaceModem3gpp *_self,
                                          gboolean enable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
    MMBroadbandModemMbim *self = MM_BROADBAND_MODEM_MBIM (_self);
    MbimDevice *device;
    GSimpleAsyncResult *result;
    MbimMessage *message;
    GAsyncReadyCallback ready_cb;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        common_enable_disable_unsolicited_events);

#define DISABLE_FEATURE G_MAXUINT32
#define AUTO_FEATURE    0

    if (enable) {
        ready_cb = (GAsyncReadyCallback)enable_signal_state_set_ready_cb;
        message = (mbim_message_signal_state_set_new (
                       30, /* signal_strength_interval */
                       DISABLE_FEATURE, /* rssi_threshold */
                       DISABLE_FEATURE, /* error_rate_threshold */
                       NULL));
    } else {
        ready_cb = (GAsyncReadyCallback)disable_signal_state_set_ready_cb;
        message = (mbim_message_signal_state_set_new (
                       DISABLE_FEATURE, /* signal_strength_interval */
                       DISABLE_FEATURE, /* rssi_threshold */
                       DISABLE_FEATURE, /* error_rate_threshold */
                       NULL));
    }

#undef DISABLE_FEATURE
#undef AUTO_FEATURE

    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         ready_cb,
                         result);
    mbim_message_unref (message);
}

static void
disable_unsolicited_events (MMIfaceModem3gpp *self,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    common_enable_disable_unsolicited_events (self, FALSE, callback, user_data);
}

static void
enable_unsolicited_events (MMIfaceModem3gpp *self,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
    common_enable_disable_unsolicited_events (self, TRUE, callback, user_data);
}

/*****************************************************************************/
/* Load operator name (3GPP interface) */

static gchar *
modem_3gpp_load_operator_name_finish (MMIfaceModem3gpp *_self,
                                      GAsyncResult *res,
                                      GError **error)
{
    MMBroadbandModemMbim *self = MM_BROADBAND_MODEM_MBIM (_self);

    if (self->priv->current_operator_name)
        return g_strdup (self->priv->current_operator_name);

    g_set_error (error,
                 MM_CORE_ERROR,
                 MM_CORE_ERROR_FAILED,
                 "Current operator name is still unknown");
    return NULL;
}

static void
modem_3gpp_load_operator_name (MMIfaceModem3gpp *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    GSimpleAsyncResult *result;

    /* Just finish the async operation */
    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_3gpp_load_operator_name);
    g_simple_async_result_set_op_res_gboolean (result, TRUE);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Load operator code (3GPP interface) */

static gchar *
modem_3gpp_load_operator_code_finish (MMIfaceModem3gpp *_self,
                                      GAsyncResult *res,
                                      GError **error)
{
    MMBroadbandModemMbim *self = MM_BROADBAND_MODEM_MBIM (_self);

    if (self->priv->current_operator_id)
        return g_strdup (self->priv->current_operator_id);

    g_set_error (error,
                 MM_CORE_ERROR,
                 MM_CORE_ERROR_FAILED,
                 "Current operator MCC/MNC is still unknown");
    return NULL;
}

static void
modem_3gpp_load_operator_code (MMIfaceModem3gpp *self,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
    GSimpleAsyncResult *result;

    /* Just finish the async operation */
    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_3gpp_load_operator_code);
    g_simple_async_result_set_op_res_gboolean (result, TRUE);
    g_simple_async_result_complete_in_idle (result);
    g_object_unref (result);
}

/*****************************************************************************/
/* Registration checks (3GPP interface) */

static gboolean
modem_3gpp_run_registration_checks_finish (MMIfaceModem3gpp *self,
                                           GAsyncResult *res,
                                           GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
register_state_query_ready (MbimDevice *device,
                            GAsyncResult *res,
                            GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimRegisterState register_state;
    MbimDataClass available_data_classes;
    gchar *provider_id;
    gchar *provider_name;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_register_state_response_parse (
            response,
            NULL, /* nw_error */
            &register_state,
            NULL, /* register_mode */
            &available_data_classes,
            NULL, /* current_cellular_class */
            &provider_id,
            &provider_name,
            NULL, /* roaming_text */
            NULL, /* registration_flag */
            NULL)) {
        MMBroadbandModemMbim *self;

        self = MM_BROADBAND_MODEM_MBIM (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
        update_registration_info (self,
                                  register_state,
                                  available_data_classes,
                                  provider_id,
                                  provider_name);
        g_object_unref (self);

        g_simple_async_result_set_op_res_gboolean (simple, TRUE);
    } else
        g_simple_async_result_take_error (simple, error);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
modem_3gpp_run_registration_checks (MMIfaceModem3gpp *self,
                                    gboolean cs_supported,
                                    gboolean ps_supported,
                                    gboolean eps_supported,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    GSimpleAsyncResult *result;
    MbimDevice *device;
    MbimMessage *message;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_3gpp_run_registration_checks);

    message = mbim_message_register_state_query_new (NULL);
    mbim_device_command (device,
                         message,
                         10,
                         NULL,
                         (GAsyncReadyCallback)register_state_query_ready,
                         result);
    mbim_message_unref (message);
}

/*****************************************************************************/

static gboolean
modem_3gpp_register_in_network_finish (MMIfaceModem3gpp *self,
                                       GAsyncResult *res,
                                       GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void
register_state_set_ready (MbimDevice *device,
                          GAsyncResult *res,
                          GSimpleAsyncResult *simple)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimNwError nw_error;

    response = mbim_device_command_finish (device, res, &error);
    if (response &&
        mbim_message_command_done_get_result (response, &error) &&
        mbim_message_register_state_response_parse (
            response,
            &nw_error,
            NULL, /* &register_state */
            NULL, /* register_mode */
            NULL, /* available_data_classes */
            NULL, /* current_cellular_class */
            NULL, /* provider_id */
            NULL, /* provider_name */
            NULL, /* roaming_text */
            NULL, /* registration_flag */
            NULL)) {
        if (nw_error)
            error = mm_mobile_equipment_error_from_mbim_nw_error (nw_error);
    }

    if (error)
        g_simple_async_result_take_error (simple, error);
    else
        g_simple_async_result_set_op_res_gboolean (simple, TRUE);

    if (response)
        mbim_message_unref (response);
    g_simple_async_result_complete (simple);
    g_object_unref (simple);
}

static void
modem_3gpp_register_in_network (MMIfaceModem3gpp *self,
                                const gchar *operator_id,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
    GSimpleAsyncResult *result;
    MbimDevice *device;
    MbimMessage *message;

    if (!peek_device (self, &device, callback, user_data))
        return;

    result = g_simple_async_result_new (G_OBJECT (self),
                                        callback,
                                        user_data,
                                        modem_3gpp_run_registration_checks);

    if (operator_id && operator_id[0])
        message = (mbim_message_register_state_set_new (
                       operator_id,
                       MBIM_REGISTER_ACTION_MANUAL,
                       0, /* data_class, none preferred */
                       NULL));
    else
        message = (mbim_message_register_state_set_new (
                       "",
                       MBIM_REGISTER_ACTION_AUTOMATIC,
                       0, /* data_class, none preferred */
                       NULL));
    mbim_device_command (device,
                         message,
                         60,
                         NULL,
                         (GAsyncReadyCallback)register_state_set_ready,
                         result);
    mbim_message_unref (message);
}

/*****************************************************************************/

MMBroadbandModemMbim *
mm_broadband_modem_mbim_new (const gchar *device,
                             const gchar **drivers,
                             const gchar *plugin,
                             guint16 vendor_id,
                             guint16 product_id)
{
    return g_object_new (MM_TYPE_BROADBAND_MODEM_MBIM,
                         MM_BASE_MODEM_DEVICE, device,
                         MM_BASE_MODEM_DRIVERS, drivers,
                         MM_BASE_MODEM_PLUGIN, plugin,
                         MM_BASE_MODEM_VENDOR_ID, vendor_id,
                         MM_BASE_MODEM_PRODUCT_ID, product_id,
                         NULL);
}

static void
mm_broadband_modem_mbim_init (MMBroadbandModemMbim *self)
{
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self),
                                              MM_TYPE_BROADBAND_MODEM_MBIM,
                                              MMBroadbandModemMbimPrivate);
}

static void
finalize (GObject *object)
{
    MMMbimPort *mbim;
    MMBroadbandModemMbim *self = MM_BROADBAND_MODEM_MBIM (object);

    g_free (self->priv->caps_device_id);
    g_free (self->priv->caps_firmware_info);
    g_free (self->priv->current_operator_id);
    g_free (self->priv->current_operator_name);

    mbim = mm_base_modem_peek_port_mbim (MM_BASE_MODEM (self));
    /* If we did open the MBIM port during initialization, close it now */
    if (mbim && mm_mbim_port_is_open (mbim)) {
        mm_mbim_port_close (mbim, NULL, NULL);
    }

    G_OBJECT_CLASS (mm_broadband_modem_mbim_parent_class)->finalize (object);
}

static void
iface_modem_init (MMIfaceModem *iface)
{
    /* Initialization steps */
    iface->load_current_capabilities = modem_load_current_capabilities;
    iface->load_current_capabilities_finish = modem_load_current_capabilities_finish;
    iface->load_modem_capabilities = NULL;
    iface->load_modem_capabilities_finish = NULL;
    iface->load_manufacturer = modem_load_manufacturer;
    iface->load_manufacturer_finish = modem_load_manufacturer_finish;
    iface->load_model = modem_load_model;
    iface->load_model_finish = modem_load_model_finish;
    iface->load_revision = modem_load_revision;
    iface->load_revision_finish = modem_load_revision_finish;
    iface->load_equipment_identifier = modem_load_equipment_identifier;
    iface->load_equipment_identifier_finish = modem_load_equipment_identifier_finish;
    iface->load_device_identifier = modem_load_device_identifier;
    iface->load_device_identifier_finish = modem_load_device_identifier_finish;
    iface->load_supported_modes = modem_load_supported_modes;
    iface->load_supported_modes_finish = modem_load_supported_modes_finish;
    iface->load_unlock_required = modem_load_unlock_required;
    iface->load_unlock_required_finish = modem_load_unlock_required_finish;
    iface->load_unlock_retries = modem_load_unlock_retries;
    iface->load_unlock_retries_finish = modem_load_unlock_retries_finish;
    iface->load_own_numbers = modem_load_own_numbers;
    iface->load_own_numbers_finish = modem_load_own_numbers_finish;
    iface->load_power_state = modem_load_power_state;
    iface->load_power_state_finish = modem_load_power_state_finish;
    iface->modem_power_up = modem_power_up;
    iface->modem_power_up_finish = common_power_up_down_finish;
    iface->modem_power_down = modem_power_down;
    iface->modem_power_down_finish = common_power_up_down_finish;

    /* Unneeded things */
    iface->modem_after_power_up = NULL;
    iface->modem_after_power_up_finish = NULL;
    iface->load_supported_charsets = NULL;
    iface->load_supported_charsets_finish = NULL;
    iface->setup_flow_control = NULL;
    iface->setup_flow_control_finish = NULL;
    iface->setup_charset = NULL;
    iface->setup_charset_finish = NULL;
    iface->load_signal_quality = NULL;
    iface->load_signal_quality_finish = NULL;

    /* Create MBIM-specific SIM */
    iface->create_sim = create_sim;
    iface->create_sim_finish = create_sim_finish;

    /* Create MBIM-specific bearer */
    iface->create_bearer = modem_create_bearer;
    iface->create_bearer_finish = modem_create_bearer_finish;
}

static void
iface_modem_3gpp_init (MMIfaceModem3gpp *iface)
{
    /* Initialization steps */
    iface->load_imei = modem_3gpp_load_imei;
    iface->load_imei_finish = modem_3gpp_load_imei_finish;
    iface->load_enabled_facility_locks = modem_3gpp_load_enabled_facility_locks;
    iface->load_enabled_facility_locks_finish = modem_3gpp_load_enabled_facility_locks_finish;

    iface->setup_unsolicited_events = setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = common_setup_cleanup_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = common_setup_cleanup_unsolicited_events_finish;
    iface->enable_unsolicited_events = enable_unsolicited_events;
    iface->enable_unsolicited_events_finish = common_enable_disable_unsolicited_events_finish;
    iface->disable_unsolicited_events = disable_unsolicited_events;
    iface->disable_unsolicited_events_finish = common_enable_disable_unsolicited_events_finish;
    iface->setup_unsolicited_registration_events = setup_unsolicited_registration_events;
    iface->setup_unsolicited_registration_events_finish = common_setup_cleanup_unsolicited_events_finish;
    iface->cleanup_unsolicited_registration_events = cleanup_unsolicited_registration_events;
    iface->cleanup_unsolicited_registration_events_finish = common_setup_cleanup_unsolicited_events_finish;
    iface->load_operator_code = modem_3gpp_load_operator_code;
    iface->load_operator_code_finish = modem_3gpp_load_operator_code_finish;
    iface->load_operator_name = modem_3gpp_load_operator_name;
    iface->load_operator_name_finish = modem_3gpp_load_operator_name_finish;
    iface->run_registration_checks = modem_3gpp_run_registration_checks;
    iface->run_registration_checks_finish = modem_3gpp_run_registration_checks_finish;
    iface->register_in_network = modem_3gpp_register_in_network;
    iface->register_in_network_finish = modem_3gpp_register_in_network_finish;

    /* Unneeded things */
    iface->enable_unsolicited_registration_events = NULL;
    iface->enable_unsolicited_registration_events_finish = NULL;
    iface->disable_unsolicited_registration_events = NULL;
    iface->disable_unsolicited_registration_events_finish = NULL;

    /* TODO: use MBIM_CID_VISIBLE_PROVIDERS */
    iface->scan_networks = NULL;
    iface->scan_networks_finish = NULL;
}

static void
mm_broadband_modem_mbim_class_init (MMBroadbandModemMbimClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBroadbandModemClass *broadband_modem_class = MM_BROADBAND_MODEM_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBroadbandModemMbimPrivate));

    object_class->finalize = finalize;

    broadband_modem_class->initialization_started = initialization_started;
    broadband_modem_class->initialization_started_finish = initialization_started_finish;
    broadband_modem_class->enabling_started = enabling_started;
    broadband_modem_class->enabling_started_finish = enabling_started_finish;
    /* Do not initialize the MBIM modem through AT commands */
    broadband_modem_class->enabling_modem_init = NULL;
    broadband_modem_class->enabling_modem_init_finish = NULL;
}
