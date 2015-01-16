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
 * Copyright (C) 2012 Google, Inc.
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

#include "mm-modem-helpers-qmi.h"
#include "mm-iface-modem.h"
#include "mm-iface-modem-messaging.h"
#include "mm-sms-qmi.h"
#include "mm-base-modem.h"
#include "mm-sms-part-3gpp.h"
#include "mm-sms-part-cdma.h"
#include "mm-log.h"

G_DEFINE_TYPE (MMSmsQmi, mm_sms_qmi, MM_TYPE_BASE_SMS)

/*****************************************************************************/

static gboolean
ensure_qmi_client (MMSmsQmi *self,
                   QmiService service,
                   QmiClient **o_client,
                   GCancellable **o_cancellable,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
    MMBaseModem *modem = NULL;
    QmiClient *client;
    MMPortQmi *port;

    g_object_get (self,
                  MM_BASE_SMS_MODEM, &modem,
                  NULL);
    g_assert (MM_IS_BASE_MODEM (modem));

    port = mm_base_modem_peek_port_qmi (modem);
    g_object_unref (modem);

    if (!port) {
        g_simple_async_report_error_in_idle (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Couldn't peek QMI port");
        return FALSE;
    }

    client = mm_port_qmi_peek_client (port,
                                      service,
                                      MM_PORT_QMI_FLAG_DEFAULT);
    if (!client) {
        g_simple_async_report_error_in_idle (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Couldn't peek client for service '%s'",
                                             qmi_service_get_string (service));
        return FALSE;
    }

    *o_cancellable = mm_base_modem_peek_cancellable (modem);
    *o_client = client;
    return TRUE;
}

/*****************************************************************************/

static gboolean
check_sms_type_support (MMSmsQmi *self,
                        MMBaseModem *modem,
                        MMSmsPart *first_part,
                        GError **error)
{
    if (MM_SMS_PART_IS_3GPP (first_part) && !mm_iface_modem_is_3gpp (MM_IFACE_MODEM (modem))) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Non-3GPP modem doesn't support 3GPP SMS");
        return FALSE;
    }

    if (MM_SMS_PART_IS_CDMA (first_part) && !mm_iface_modem_is_cdma (MM_IFACE_MODEM (modem))) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Non-CDMA modem doesn't support CDMA SMS");
        return FALSE;
    }

    return TRUE;
}

/*****************************************************************************/
/* Store the SMS */

typedef struct {
    MMBaseSms *self;
    MMBaseModem *modem;
    QmiClientWms *client;
    GSimpleAsyncResult *result;
    MMSmsStorage storage;
    GList *current;
    GCancellable *cancellable;
} SmsStoreContext;

static void
sms_store_context_complete_and_free (SmsStoreContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    g_object_unref (ctx->result);
    g_object_unref (ctx->client);
    g_object_unref (ctx->modem);
    g_object_unref (ctx->self);
    g_slice_free (SmsStoreContext, ctx);
}

static gboolean
sms_store_finish (MMBaseSms *self,
                  GAsyncResult *res,
                  GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void sms_store_next_part (SmsStoreContext *ctx);

static void
store_ready (QmiClientWms *client,
             GAsyncResult *res,
             SmsStoreContext *ctx)
{
    QmiMessageWmsRawWriteOutput *output = NULL;
    GError *error = NULL;
    GList *parts;
    guint32 idx;

    output = qmi_client_wms_raw_write_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
        sms_store_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_wms_raw_write_output_get_result (output, &error)) {
        qmi_message_wms_raw_write_output_unref (output);
        g_prefix_error (&error, "Couldn't write SMS part: ");
        g_simple_async_result_take_error (ctx->result, error);
        sms_store_context_complete_and_free (ctx);
        return;
    }

    qmi_message_wms_raw_write_output_get_memory_index (
        output,
        &idx,
        NULL);
    qmi_message_wms_raw_write_output_unref (output);

    /* Set the index in the part we hold */
    parts = mm_base_sms_get_parts (ctx->self);
    mm_sms_part_set_index ((MMSmsPart *)parts->data, (guint)idx);

    /* Go on with next one */
    ctx->current = g_list_next (ctx->current);
    sms_store_next_part (ctx);
}

static void
sms_store_next_part (SmsStoreContext *ctx)
{
    QmiMessageWmsRawWriteInput *input;
    guint8 *pdu = NULL;
    guint pdulen = 0;
    guint msgstart = 0;
    GArray *array;
    GError *error = NULL;

    if (!ctx->current) {
        /* Done we are */
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
        sms_store_context_complete_and_free (ctx);
        return;
    }

    /* Get PDU */
    if (MM_SMS_PART_IS_3GPP ((MMSmsPart *)ctx->current->data))
        pdu = mm_sms_part_3gpp_get_submit_pdu ((MMSmsPart *)ctx->current->data, &pdulen, &msgstart, &error);
    else if (MM_SMS_PART_IS_CDMA ((MMSmsPart *)ctx->current->data))
        pdu = mm_sms_part_cdma_get_submit_pdu ((MMSmsPart *)ctx->current->data, &pdulen, &error);

    if (!pdu) {
        if (error)
            g_simple_async_result_take_error (ctx->result, error);
        else
            g_simple_async_result_set_error (ctx->result,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Unknown or unsupported PDU type in SMS part: %s",
                                             mm_sms_pdu_type_get_string (
                                                 mm_sms_part_get_pdu_type (
                                                     (MMSmsPart *)ctx->current->data)));
        sms_store_context_complete_and_free (ctx);
        return;
    }

    /* Convert to GArray */
    array = g_array_append_vals (g_array_sized_new (FALSE, FALSE, sizeof (guint8), pdulen),
                                 pdu,
                                 pdulen);
    g_free (pdu);

    /* Create input bundle and send the QMI request */
    input = qmi_message_wms_raw_write_input_new ();
    qmi_message_wms_raw_write_input_set_raw_message_data (
        input,
        mm_sms_storage_to_qmi_storage_type (ctx->storage),
        (MM_SMS_PART_IS_3GPP ((MMSmsPart *)ctx->current->data) ?
         QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_POINT_TO_POINT :
         QMI_WMS_MESSAGE_FORMAT_CDMA),
        array,
        NULL);
    qmi_client_wms_raw_write (ctx->client,
                              input,
                              5,
                              ctx->cancellable,
                              (GAsyncReadyCallback)store_ready,
                              ctx);
    qmi_message_wms_raw_write_input_unref (input);
    g_array_unref (array);
}

static void
sms_store (MMBaseSms *self,
           MMSmsStorage storage,
           GAsyncReadyCallback callback,
           gpointer user_data)
{
    SmsStoreContext *ctx;
    QmiClient *client = NULL;
    GError *error = NULL;
    GCancellable *cancellable = NULL;

    /* Ensure WMS client */
    if (!ensure_qmi_client (MM_SMS_QMI (self),
                            QMI_SERVICE_WMS, &client,
                            &cancellable,
                            callback, user_data))
        return;

    /* Setup the context */
    ctx = g_slice_new0 (SmsStoreContext);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             sms_store);
    ctx->self = g_object_ref (self);
    ctx->client = g_object_ref (client);
    ctx->storage = storage;
    ctx->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
    g_object_get (self,
                  MM_BASE_SMS_MODEM, &ctx->modem,
                  NULL);

    ctx->current = mm_base_sms_get_parts (self);

    /* Check whether we support the given SMS type */
    if (!check_sms_type_support (MM_SMS_QMI (self), ctx->modem, (MMSmsPart *)ctx->current->data, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        sms_store_context_complete_and_free (ctx);
        return;
    }

    /* Go on */
    sms_store_next_part (ctx);
}

/*****************************************************************************/
/* Send the SMS */

typedef struct {
    MMBaseSms *self;
    MMBaseModem *modem;
    QmiClientWms *client;
    GSimpleAsyncResult *result;
    gboolean from_storage;
    GList *current;
    GCancellable *cancellable;
} SmsSendContext;

static void
sms_send_context_complete_and_free (SmsSendContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    g_object_unref (ctx->client);
    g_object_unref (ctx->modem);
    g_object_unref (ctx->self);
    g_slice_free (SmsSendContext, ctx);
}

static gboolean
sms_send_finish (MMBaseSms *self,
                 GAsyncResult *res,
                 GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void sms_send_next_part (SmsSendContext *ctx);

static void
send_generic_ready (QmiClientWms *client,
                    GAsyncResult *res,
                    SmsSendContext *ctx)
{
    QmiMessageWmsRawSendOutput *output = NULL;
    GError *error = NULL;
    guint16 message_id;

    output = qmi_client_wms_raw_send_finish (client, res, &error);
    if (!output) {
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
        sms_send_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_wms_raw_send_output_get_result (output, &error)) {
        QmiWmsGsmUmtsRpCause rp_cause;
        QmiWmsGsmUmtsTpCause tp_cause;

        if (qmi_message_wms_raw_send_output_get_gsm_wcdma_cause_info (
                output,
                &rp_cause,
                &tp_cause,
                NULL)) {
            mm_warn ("Couldn't send SMS; RP cause (%u): '%s'; TP cause (%u): '%s'",
                     rp_cause,
                     qmi_wms_gsm_umts_rp_cause_get_string (rp_cause),
                     tp_cause,
                     qmi_wms_gsm_umts_tp_cause_get_string (tp_cause));
        }
        qmi_message_wms_raw_send_output_unref (output);

        g_prefix_error (&error, "Couldn't write SMS part: ");
        g_simple_async_result_take_error (ctx->result, error);
        sms_send_context_complete_and_free (ctx);
        return;
    }

    if (qmi_message_wms_raw_send_output_get_message_id (output, &message_id, NULL))
        mm_sms_part_set_message_reference ((MMSmsPart *)ctx->current->data,
                                           message_id);

    qmi_message_wms_raw_send_output_unref (output);

    /* Go on with next part */
    ctx->current = g_list_next (ctx->current);
    sms_send_next_part (ctx);
}

static void
sms_send_generic (SmsSendContext *ctx)
{
    QmiMessageWmsRawSendInput *input;
    guint8 *pdu = NULL;
    guint pdulen = 0;
    guint msgstart = 0;
    GArray *array;
    GError *error = NULL;

    /* Get PDU */
    if (MM_SMS_PART_IS_3GPP ((MMSmsPart *)ctx->current->data))
        pdu = mm_sms_part_3gpp_get_submit_pdu ((MMSmsPart *)ctx->current->data, &pdulen, &msgstart, &error);
    else if (MM_SMS_PART_IS_CDMA ((MMSmsPart *)ctx->current->data))
        pdu = mm_sms_part_cdma_get_submit_pdu ((MMSmsPart *)ctx->current->data, &pdulen, &error);

    if (!pdu) {
        if (error)
            g_simple_async_result_take_error (ctx->result, error);
        else
            g_simple_async_result_set_error (ctx->result,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Unknown or unsupported PDU type in SMS part: %s",
                                             mm_sms_pdu_type_get_string (
                                                 mm_sms_part_get_pdu_type (
                                                     (MMSmsPart *)ctx->current->data)));
        sms_send_context_complete_and_free (ctx);
        return;
    }

    /* Convert to GArray */
    array = g_array_append_vals (g_array_sized_new (FALSE, FALSE, sizeof (guint8), pdulen),
                                 pdu,
                                 pdulen);
    g_free (pdu);

    input = qmi_message_wms_raw_send_input_new ();
    qmi_message_wms_raw_send_input_set_raw_message_data (
        input,
        (MM_SMS_PART_IS_3GPP ((MMSmsPart *)ctx->current->data) ?
         QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_POINT_TO_POINT :
         QMI_WMS_MESSAGE_FORMAT_CDMA),
        array,
        NULL);

    qmi_client_wms_raw_send (ctx->client,
                             input,
                             30,
                             ctx->cancellable,
                             (GAsyncReadyCallback)send_generic_ready,
                             ctx);
    qmi_message_wms_raw_send_input_unref (input);
    g_array_unref (array);
}

static void
send_from_storage_ready (QmiClientWms *client,
                         GAsyncResult *res,
                         SmsSendContext *ctx)
{
    QmiMessageWmsSendFromMemoryStorageOutput *output = NULL;
    GError *error = NULL;
    guint16 message_id;

    output = qmi_client_wms_send_from_memory_storage_finish (client, res, &error);
    if (!output) {
        if (g_error_matches (error,
                             QMI_CORE_ERROR,
                             QMI_CORE_ERROR_UNSUPPORTED)) {
            mm_dbg ("Couldn't send SMS from storage: '%s'; trying generic send...",
                    error->message);
            g_error_free (error);
            ctx->from_storage = FALSE;
            sms_send_next_part (ctx);
            return;
        }

        /* Fatal error */
        g_prefix_error (&error, "QMI operation failed: ");
        g_simple_async_result_take_error (ctx->result, error);
        sms_send_context_complete_and_free (ctx);
        return;
    }

    if (!qmi_message_wms_send_from_memory_storage_output_get_result (output, &error)) {
        if (g_error_matches (error,
                             QMI_PROTOCOL_ERROR,
                             QMI_PROTOCOL_ERROR_INVALID_QMI_COMMAND)) {
            mm_dbg ("Couldn't send SMS from storage: '%s'; trying generic send...",
                    error->message);
            g_error_free (error);
            ctx->from_storage = FALSE;
            sms_send_next_part (ctx);
        } else {
            QmiWmsGsmUmtsRpCause rp_cause;
            QmiWmsGsmUmtsTpCause tp_cause;
            QmiWmsCdmaCauseCode cdma_cause_code;
            QmiWmsCdmaErrorClass cdma_error_class;

            if (qmi_message_wms_send_from_memory_storage_output_get_gsm_wcdma_cause_info (
                    output,
                    &rp_cause,
                    &tp_cause,
                    NULL)) {
                mm_warn ("Couldn't send SMS; RP cause (%u): '%s'; TP cause (%u): '%s'",
                         rp_cause,
                         qmi_wms_gsm_umts_rp_cause_get_string (rp_cause),
                         tp_cause,
                         qmi_wms_gsm_umts_tp_cause_get_string (tp_cause));
            }

            if (qmi_message_wms_send_from_memory_storage_output_get_cdma_cause_code (
                    output,
                    &cdma_cause_code,
                    NULL)) {
                mm_warn ("Couldn't send SMS; cause code (%u): '%s'",
                         cdma_cause_code,
                         qmi_wms_cdma_cause_code_get_string (cdma_cause_code));
            }

            if (qmi_message_wms_send_from_memory_storage_output_get_cdma_error_class (
                    output,
                    &cdma_error_class,
                    NULL)) {
                mm_warn ("Couldn't send SMS; error class (%u): '%s'",
                         cdma_error_class,
                         qmi_wms_cdma_error_class_get_string (cdma_error_class));
            }

            g_prefix_error (&error, "Couldn't write SMS part: ");
            g_simple_async_result_take_error (ctx->result, error);
            sms_send_context_complete_and_free (ctx);
        }

        qmi_message_wms_send_from_memory_storage_output_unref (output);
        return;
    }

    if (qmi_message_wms_send_from_memory_storage_output_get_message_id (output, &message_id, NULL))
        mm_sms_part_set_message_reference ((MMSmsPart *)ctx->current->data,
                                           message_id);

    qmi_message_wms_send_from_memory_storage_output_unref (output);

    /* Go on with next part */
    ctx->current = g_list_next (ctx->current);
    sms_send_next_part (ctx);
}

static void
sms_send_from_storage (SmsSendContext *ctx)
{
    QmiMessageWmsSendFromMemoryStorageInput *input;

    input = qmi_message_wms_send_from_memory_storage_input_new ();

    qmi_message_wms_send_from_memory_storage_input_set_information (
        input,
        mm_sms_storage_to_qmi_storage_type (mm_base_sms_get_storage (ctx->self)),
        mm_sms_part_get_index ((MMSmsPart *)ctx->current->data),
        (MM_SMS_PART_IS_3GPP ((MMSmsPart *)ctx->current->data) ?
         QMI_WMS_MESSAGE_MODE_GSM_WCDMA :
         QMI_WMS_MESSAGE_MODE_CDMA),
        NULL);

    qmi_client_wms_send_from_memory_storage (
        ctx->client,
        input,
        30,
        ctx->cancellable,
        (GAsyncReadyCallback)send_from_storage_ready,
        ctx);
    qmi_message_wms_send_from_memory_storage_input_unref (input);
}

static void
sms_send_next_part (SmsSendContext *ctx)
{
    if (!ctx->current) {
        /* Done we are */
        g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);
        sms_send_context_complete_and_free (ctx);
        return;
    }

    /* Send from storage? */
    if (ctx->from_storage)
        sms_send_from_storage (ctx);
    else
        sms_send_generic (ctx);
}

static void
sms_send (MMBaseSms *self,
          GAsyncReadyCallback callback,
          gpointer user_data)
{
    SmsSendContext *ctx;
    QmiClient *client = NULL;
    GError *error = NULL;
    GCancellable *cancellable = NULL;

    /* Ensure WMS client */
    if (!ensure_qmi_client (MM_SMS_QMI (self),
                            QMI_SERVICE_WMS, &client,
                            &cancellable,
                            callback, user_data))
        return;

    /* Setup the context */
    ctx = g_slice_new0 (SmsSendContext);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             sms_send);
    ctx->self = g_object_ref (self);
    ctx->client = g_object_ref (client);
    ctx->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
    g_object_get (self,
                  MM_BASE_SMS_MODEM, &ctx->modem,
                  NULL);

    /* If the SMS is STORED, try to send from storage */
    ctx->from_storage = (mm_base_sms_get_storage (self) != MM_SMS_STORAGE_UNKNOWN);

    ctx->current = mm_base_sms_get_parts (self);

    /* Check whether we support the given SMS type */
    if (!check_sms_type_support (MM_SMS_QMI (self), ctx->modem, (MMSmsPart *)ctx->current->data, &error)) {
        g_simple_async_result_take_error (ctx->result, error);
        sms_send_context_complete_and_free (ctx);
        return;
    }

    sms_send_next_part (ctx);
}

/*****************************************************************************/

typedef struct {
    MMBaseSms *self;
    MMBaseModem *modem;
    QmiClientWms *client;
    GSimpleAsyncResult *result;
    GList *current;
    guint n_failed;
    GCancellable *cancellable;
} SmsDeletePartsContext;

static void
sms_delete_parts_context_complete_and_free (SmsDeletePartsContext *ctx)
{
    g_simple_async_result_complete_in_idle (ctx->result);
    g_object_unref (ctx->result);
    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    g_object_unref (ctx->client);
    g_object_unref (ctx->modem);
    g_object_unref (ctx->self);
    g_slice_free (SmsDeletePartsContext, ctx);
}

static gboolean
sms_delete_finish (MMBaseSms *self,
                   GAsyncResult *res,
                   GError **error)
{
    return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
}

static void delete_next_part (SmsDeletePartsContext *ctx);

static void
delete_part_ready (QmiClientWms *client,
                   GAsyncResult *res,
                   SmsDeletePartsContext *ctx)
{
    QmiMessageWmsDeleteOutput *output = NULL;
    GError *error = NULL;

    output = qmi_client_wms_delete_finish (client, res, &error);
    if (!output) {
        ctx->n_failed++;
        mm_dbg ("QMI operation failed: Couldn't delete SMS part with index %u: '%s'",
                mm_sms_part_get_index ((MMSmsPart *)ctx->current->data),
                error->message);
        g_error_free (error);
    } else if (!qmi_message_wms_delete_output_get_result (output, &error)) {
        ctx->n_failed++;
        mm_dbg ("Couldn't delete SMS part with index %u: '%s'",
                mm_sms_part_get_index ((MMSmsPart *)ctx->current->data),
                error->message);
        g_error_free (error);
    }

    if (output)
        qmi_message_wms_delete_output_unref (output);

    /* We reset the index, as there is no longer that part */
    mm_sms_part_set_index ((MMSmsPart *)ctx->current->data, SMS_PART_INVALID_INDEX);

    ctx->current = g_list_next (ctx->current);
    delete_next_part (ctx);
}

static void
delete_next_part (SmsDeletePartsContext *ctx)
{
    QmiMessageWmsDeleteInput *input;

    /* Skip non-stored parts */
    while (ctx->current &&
           mm_sms_part_get_index ((MMSmsPart *)ctx->current->data) == SMS_PART_INVALID_INDEX)
        ctx->current = g_list_next (ctx->current);

    /* If all removed, we're done */
    if (!ctx->current) {
        if (ctx->n_failed > 0)
            g_simple_async_result_set_error (ctx->result,
                                             MM_CORE_ERROR,
                                             MM_CORE_ERROR_FAILED,
                                             "Couldn't delete %u parts from this SMS",
                                             ctx->n_failed);
        else
            g_simple_async_result_set_op_res_gboolean (ctx->result, TRUE);

        sms_delete_parts_context_complete_and_free (ctx);
        return;
    }

    input = qmi_message_wms_delete_input_new ();
    qmi_message_wms_delete_input_set_memory_storage (
        input,
        mm_sms_storage_to_qmi_storage_type (mm_base_sms_get_storage (ctx->self)),
        NULL);
    qmi_message_wms_delete_input_set_memory_index (
        input,
        (guint32)mm_sms_part_get_index ((MMSmsPart *)ctx->current->data),
        NULL);
    qmi_message_wms_delete_input_set_message_mode (
        input,
        (MM_SMS_PART_IS_3GPP ((MMSmsPart *)ctx->current->data) ?
         QMI_WMS_MESSAGE_MODE_GSM_WCDMA:
         QMI_WMS_MESSAGE_MODE_CDMA),
        NULL);
    qmi_client_wms_delete (ctx->client,
                           input,
                           5,
                           ctx->cancellable,
                           (GAsyncReadyCallback)delete_part_ready,
                           ctx);
    qmi_message_wms_delete_input_unref (input);
}

static void
sms_delete (MMBaseSms *self,
            GAsyncReadyCallback callback,
            gpointer user_data)
{
    SmsDeletePartsContext *ctx;
    QmiClient *client = NULL;
    GCancellable *cancellable = NULL;

    /* Ensure WMS client */
    if (!ensure_qmi_client (MM_SMS_QMI (self),
                            QMI_SERVICE_WMS, &client,
                            &cancellable,
                            callback, user_data))
        return;

    ctx = g_slice_new0 (SmsDeletePartsContext);
    ctx->result = g_simple_async_result_new (G_OBJECT (self),
                                             callback,
                                             user_data,
                                             sms_delete);
    ctx->self = g_object_ref (self);
    ctx->client = g_object_ref (client);
    ctx->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
    g_object_get (self,
                  MM_BASE_SMS_MODEM, &ctx->modem,
                  NULL);

    /* Go on deleting parts */
    ctx->current = mm_base_sms_get_parts (self);
    delete_next_part (ctx);
}

/*****************************************************************************/

MMBaseSms *
mm_sms_qmi_new (MMBaseModem *modem)
{
    return MM_BASE_SMS (g_object_new (MM_TYPE_SMS_QMI,
                                      MM_BASE_SMS_MODEM, modem,
                                      NULL));
}

static void
mm_sms_qmi_init (MMSmsQmi *self)
{
}

static void
mm_sms_qmi_class_init (MMSmsQmiClass *klass)
{
    MMBaseSmsClass *base_sms_class = MM_BASE_SMS_CLASS (klass);

    base_sms_class->store = sms_store;
    base_sms_class->store_finish = sms_store_finish;
    base_sms_class->send = sms_send;
    base_sms_class->send_finish = sms_send_finish;
    base_sms_class->delete = sms_delete;
    base_sms_class->delete_finish = sms_delete_finish;
}
