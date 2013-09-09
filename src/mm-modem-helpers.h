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
 * Copyright (C) 2012 Google, Inc.
 */

#ifndef MM_MODEM_HELPERS_H
#define MM_MODEM_HELPERS_H

#include <ModemManager.h>

#include "glib-object.h"
#include "mm-charsets.h"

/* NOTE:
 * We will use the following nomenclature for the different AT commands referred
 *  - AT+SOMETHING       --> "Exec" command
 *  - AT+SOMETHING?      --> "Read" command
 *  - AT+SOMETHING=X,X   --> "Write" command
 *  - AT+SOMETHING=?     --> "Test" command
 */


/*****************************************************************************/
/* Common utilities */
/*****************************************************************************/

#define MM_MODEM_CAPABILITY_3GPP_LTE    \
    (MM_MODEM_CAPABILITY_LTE |          \
     MM_MODEM_CAPABILITY_LTE_ADVANCED)

#define MM_MODEM_CAPABILITY_3GPP        \
    (MM_MODEM_CAPABILITY_GSM_UMTS |     \
     MM_MODEM_CAPABILITY_3GPP_LTE)

gchar       *mm_strip_quotes (gchar *str);
const gchar *mm_strip_tag    (const gchar *str,
                              const gchar *cmd);

guint mm_count_bits_set (gulong number);

gchar *mm_create_device_identifier (guint vid,
                                    guint pid,
                                    const gchar *ati,
                                    const gchar *ati1,
                                    const gchar *gsn,
                                    const gchar *revision,
                                    const gchar *model,
                                    const gchar *manf);

guint mm_netmask_to_cidr (const gchar *netmask);

GArray *mm_filter_current_bands (const GArray *supported_bands,
                                 const GArray *current_bands);

gchar *mm_new_iso8601_time (guint year,
                            guint month,
                            guint day,
                            guint hour,
                            guint minute,
                            guint second,
                            gboolean have_offset,
                            gint offset_minutes);

GArray *mm_filter_supported_modes (const GArray *all,
                                   const GArray *supported_combinations);

GArray *mm_filter_supported_capabilities (MMModemCapability all,
                                          const GArray *supported_combinations);

/*****************************************************************************/
/* 3GPP specific helpers and utilities */
/*****************************************************************************/

/* Common Regex getters */
GPtrArray *mm_3gpp_creg_regex_get     (gboolean solicited);
void       mm_3gpp_creg_regex_destroy (GPtrArray *array);
GRegex    *mm_3gpp_ciev_regex_get (void);
GRegex    *mm_3gpp_cusd_regex_get (void);
GRegex    *mm_3gpp_cmti_regex_get (void);
GRegex    *mm_3gpp_cds_regex_get (void);


/* AT+COPS=? (network scan) response parser */
typedef struct {
    MMModem3gppNetworkAvailability status;
    gchar *operator_long;
    gchar *operator_short;
    gchar *operator_code; /* mandatory */
    MMModemAccessTechnology access_tech;
} MM3gppNetworkInfo;
void mm_3gpp_network_info_list_free (GList *info_list);
GList *mm_3gpp_parse_cops_test_response (const gchar *reply,
                                         GError **error);

/* AT+CGDCONT=? (PDP context format) test parser */
typedef struct {
    guint min_cid;
    guint max_cid;
    MMBearerIpFamily pdp_type;
} MM3gppPdpContextFormat;
void mm_3gpp_pdp_context_format_list_free (GList *pdp_format_list);
GList *mm_3gpp_parse_cgdcont_test_response (const gchar *reply,
                                            GError **error);

/* AT+CGDCONT? (PDP context query) response parser */
typedef struct {
    guint cid;
    MMBearerIpFamily pdp_type;
    gchar *apn;
} MM3gppPdpContext;
void mm_3gpp_pdp_context_list_free (GList *pdp_list);
GList *mm_3gpp_parse_cgdcont_read_response (const gchar *reply,
                                            GError **error);

/* CREG/CGREG response/unsolicited message parser */
gboolean mm_3gpp_parse_creg_response (GMatchInfo *info,
                                      MMModem3gppRegistrationState *out_reg_state,
                                      gulong *out_lac,
                                      gulong *out_ci,
                                      MMModemAccessTechnology *out_act,
                                      gboolean *out_cgreg,
                                      gboolean *out_cereg,
                                      GError **error);

/* AT+CMGF=? (SMS message format) response parser */
gboolean mm_3gpp_parse_cmgf_test_response (const gchar *reply,
                                           gboolean *sms_pdu_supported,
                                           gboolean *sms_text_supported,
                                           GError **error);

/* AT+CPMS=? (Preferred SMS storage) response parser */
gboolean mm_3gpp_parse_cpms_test_response (const gchar *reply,
                                           GArray **mem1,
                                           GArray **mem2,
                                           GArray **mem3);

/* AT+CSCS=? (Supported charsets) response parser */
gboolean mm_3gpp_parse_cscs_test_response (const gchar *reply,
                                           MMModemCharset *out_charsets);

/* AT+CLCK=? (Supported locks) response parser */
gboolean mm_3gpp_parse_clck_test_response (const gchar *reply,
                                           MMModem3gppFacility *out_facilities);

/* AT+CLCK=X,X,X... (Current locks) response parser */
gboolean mm_3gpp_parse_clck_write_response (const gchar *reply,
                                            gboolean *enabled);

/* AT+CNUM (Own numbers) response parser */
GStrv mm_3gpp_parse_cnum_exec_response (const gchar *reply,
                                        GError **error);

/* AT+CIND=? (Supported indicators) response parser */
typedef struct MM3gppCindResponse MM3gppCindResponse;
GHashTable  *mm_3gpp_parse_cind_test_response    (const gchar *reply,
                                                  GError **error);
const gchar *mm_3gpp_cind_response_get_desc      (MM3gppCindResponse *r);
guint        mm_3gpp_cind_response_get_index     (MM3gppCindResponse *r);
gint         mm_3gpp_cind_response_get_min       (MM3gppCindResponse *r);
gint         mm_3gpp_cind_response_get_max       (MM3gppCindResponse *r);

/* AT+CIND? (Current indicators) response parser */
GByteArray *mm_3gpp_parse_cind_read_response (const gchar *reply,
                                              GError **error);

/* AT+CMGL=4 (list sms parts) response parser */
typedef struct {
    gint index;
    gint status;
    gchar *pdu;
} MM3gppPduInfo;
void   mm_3gpp_pdu_info_list_free      (GList *info_list);
GList *mm_3gpp_parse_pdu_cmgl_response (const gchar *str,
                                        GError **error);


/* Additional 3GPP-specific helpers */

MMModem3gppFacility mm_3gpp_acronym_to_facility (const gchar *str);
gchar *mm_3gpp_facility_to_acronym (MMModem3gppFacility facility);

MMModemAccessTechnology mm_string_to_access_tech (const gchar *string);

gchar *mm_3gpp_parse_operator (const gchar *reply,
                               MMModemCharset cur_charset);

gboolean mm_3gpp_parse_operator_id (const gchar *operator_id,
                                    guint16 *mcc,
                                    guint16 *mnc,
                                    GError **error);

const gchar      *mm_3gpp_get_pdp_type_from_ip_family (MMBearerIpFamily family);
MMBearerIpFamily  mm_3gpp_get_ip_family_from_pdp_type (const gchar *pdp_type);

char *mm_3gpp_parse_iccid (const char *raw_iccid, gboolean swap, GError **error);

/*****************************************************************************/
/* CDMA specific helpers and utilities */
/*****************************************************************************/

/* AT+SPSERVICE? response parser */
gboolean mm_cdma_parse_spservice_read_response (const gchar *reply,
                                                MMModemCdmaRegistrationState *out_cdma_1x_state,
                                                MMModemCdmaRegistrationState *out_evdo_state);

/* Generic ERI response parser */
gboolean mm_cdma_parse_eri (const gchar *reply,
                            gboolean *out_roaming,
                            guint32 *out_ind,
                            const gchar **out_desc);

/* AT+CRM=? response parser */
gboolean mm_cdma_parse_crm_test_response (const gchar *reply,
                                          MMModemCdmaRmProtocol *min,
                                          MMModemCdmaRmProtocol *max,
                                          GError **error);

/* Additional CDMA-specific helpers */

#define MM_MODEM_CDMA_SID_UNKNOWN 99999
#define MM_MODEM_CDMA_NID_UNKNOWN 99999

MMModemCdmaRmProtocol mm_cdma_get_rm_protocol_from_index (guint index,
                                                          GError **error);
guint mm_cdma_get_index_from_rm_protocol (MMModemCdmaRmProtocol protocol,
                                          GError **error);

gint  mm_cdma_normalize_class (const gchar *orig_class);
gchar mm_cdma_normalize_band  (const gchar *long_band,
                               gint *out_class);

gboolean mm_parse_gsn (const char *gsn,
                       gchar **out_imei,
                       gchar **out_meid,
                       gchar **out_esn);

#endif  /* MM_MODEM_HELPERS_H */
