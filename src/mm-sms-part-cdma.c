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
 * Copyright (C) 2013 Google, Inc.
 */

#include <ctype.h>
#include <string.h>

#include <glib.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-charsets.h"
#include "mm-sms-part-cdma.h"
#include "mm-log.h"

/*
 * Documentation that you may want to have around:
 *
 *   3GPP2 C.S0015-B: Short Message Service (SMS) for Wideband Spread Spectrum
 *                    Systems.
 *
 *   3GPP2 C.R1001-G: Administration of Parameter Value Assignments for CDMA2000
 *                    Spread Spectrum Standards.
 *
 *   3GPP2 X.S0004-550-E: Mobile Application Part (MAP).
 *
 *   3GPP2 C.S0005-E: Upper Layer (Layer 3) Signaling Standard for CDMA2000
 *                    Spread Spectrum Systems.
 *
 *   3GPP2 N.S0005-O: Cellular Radiotelecommunications Intersystem Operations.
 */

/* 3GPP2 C.S0015-B, section 3.4, table 3.4-1 */
typedef enum {
    MESSAGE_TYPE_POINT_TO_POINT = 0,
    MESSAGE_TYPE_BROADCAST      = 1,
    MESSAGE_TYPE_ACKNOWLEDGE    = 2
} MessageType;

/* 3GPP2 C.S0015-B, section 3.4.3, table 3.4.3-1 */
typedef enum {
    PARAMETER_ID_TELESERVICE_ID         = 0,
    PARAMETER_ID_SERVICE_CATEGORY       = 1,
    PARAMETER_ID_ORIGINATING_ADDRESS    = 2,
    PARAMETER_ID_ORIGINATING_SUBADDRESS = 3,
    PARAMETER_ID_DESTINATION_ADDRESS    = 4,
    PARAMETER_ID_DESTINATION_SUBADDRESS = 5,
    PARAMETER_ID_BEARER_REPLY_OPTION    = 6,
    PARAMETER_ID_CAUSE_CODES            = 7,
    PARAMETER_ID_BEARER_DATA            = 8
} ParameterId;

/* 3GPP2 C.S0015-B, section 3.4.3.3 */
typedef enum {
    DIGIT_MODE_DTMF  = 0,
    DIGIT_MODE_ASCII = 1
} DigitMode;

/* 3GPP2 C.S0015-B, section 3.4.3.3 */
typedef enum {
    NUMBER_MODE_DIGIT                = 0,
    NUMBER_MODE_DATA_NETWORK_ADDRESS = 1
} NumberMode;

/* 3GPP2 C.S0005-E, section 2.7.1.3.2.4, table 2.7.1.3.2.4-2 */
typedef enum {
    NUMBER_TYPE_UNKNOWN          = 0,
    NUMBER_TYPE_INTERNATIONAL    = 1,
    NUMBER_TYPE_NATIONAL         = 2,
    NUMBER_TYPE_NETWORK_SPECIFIC = 3,
    NUMBER_TYPE_SUBSCRIBER       = 4,
    /* 5 reserved */
    NUMBER_TYPE_ABBREVIATED      = 6,
    /* 7 reserved */
} NumberType;

/* 3GPP2 C.S0015-B, section 3.4.3.3, table 3.4.3.3-1 */
typedef enum {
    DATA_NETWORK_ADDRESS_TYPE_UNKNOWN                = 0,
    DATA_NETWORK_ADDRESS_TYPE_INTERNET_PROTOCOL      = 1,
    DATA_NETWORK_ADDRESS_TYPE_INTERNET_EMAIL_ADDRESS = 2
} DataNetworkAddressType;

/* 3GPP2 C.S0005-E, section 2.7.1.3.2.4, table 2.7.1.3.2.4-3 */
typedef enum {
    NUMBERING_PLAN_UNKNOWN = 0,
    NUMBERING_PLAN_ISDN    = 1,
    NUMBERING_PLAN_DATA    = 3,
    NUMBERING_PLAN_TELEX   = 4,
    NUMBERING_PLAN_PRIVATE = 9,
    /* 15 reserved */
} NumberingPlan;

/* 3GPP2 C.S0015-B, section 3.4.3.6 */
typedef enum {
    ERROR_CLASS_NO_ERROR  = 0,
    /* 1 reserved */
    ERROR_CLASS_TEMPORARY = 2,
    ERROR_CLASS_PERMANENT = 3
} ErrorClass;

/* 3GPP2 N.S0005-O, section 6.5.2.125*/
typedef enum {
    CAUSE_CODE_NETWORK_PROBLEM_ADDRESS_VACANT              = 0,
    CAUSE_CODE_NETWORK_PROBLEM_ADDRESS_TRANSLATION_FAILURE = 1,
    CAUSE_CODE_NETWORK_PROBLEM_NETWORK_RESOURCE_OUTAGE     = 2,
    CAUSE_CODE_NETWORK_PROBLEM_NETWORK_FAILURE             = 3,
    CAUSE_CODE_NETWORK_PROBLEM_INVALID_TELESERVICE_ID      = 4,
    CAUSE_CODE_NETWORK_PROBLEM_OTHER                       = 5,
    /* 6 to 31 reserved, treat as CAUSE_CODE_NETWORK_PROBLEM_OTHER */
    CAUSE_CODE_TERMINAL_PROBLEM_NO_PAGE_RESPONSE                      = 32,
    CAUSE_CODE_TERMINAL_PROBLEM_DESTINATION_BUSY                      = 33,
    CAUSE_CODE_TERMINAL_PROBLEM_NO_ACKNOWLEDGMENT                     = 34,
    CAUSE_CODE_TERMINAL_PROBLEM_DESTINATION_RESOURCE_SHORTAGE         = 35,
    CAUSE_CODE_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED                = 36,
    CAUSE_CODE_TERMINAL_PROBLEM_DESTINATION_OUT_OF_SERVICE            = 37,
    CAUSE_CODE_TERMINAL_PROBLEM_DESTINATION_NO_LONGER_AT_THIS_ADDRESS = 38,
    CAUSE_CODE_TERMINAL_PROBLEM_OTHER                                 = 39,
    /* 40 to 47 reserved, treat as CAUSE_CODE_TERMINAL_PROBLEM_OTHER */
    /* 48 to 63 reserved, treat as CAUSE_CODE_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED */
    CAUSE_CODE_RADIO_INTERFACE_PROBLEM_RESOURCE_SHORTAGE = 64,
    CAUSE_CODE_RADIO_INTERFACE_PROBLEM_INCOMPATIBILITY   = 65,
    CAUSE_CODE_RADIO_INTERFACE_PROBLEM_OTHER             = 66,
    /* 67 to 95 reserved, treat as CAUSE_CODE_RADIO_INTERFACE_PROBLEM_OTHER */
    CAUSE_CODE_GENERAL_PROBLEM_ENCODING                            = 96,
    CAUSE_CODE_GENERAL_PROBLEM_SMS_ORIGINATION_DENIED              = 97,
    CAUSE_CODE_GENERAL_PROBLEM_SMS_TERMINATION_DENIED              = 98,
    CAUSE_CODE_GENERAL_PROBLEM_SUPPLEMENTARY_SERVICE_NOT_SUPPORTED = 99,
    CAUSE_CODE_GENERAL_PROBLEM_SMS_NOT_SUPPORTED                   = 100,
    /* 101 reserved */
    CAUSE_CODE_GENERAL_PROBLEM_MISSING_EXPECTED_PARAMETER   = 102,
    CAUSE_CODE_GENERAL_PROBLEM_MISSING_MANDATORY_PARAMETER  = 103,
    CAUSE_CODE_GENERAL_PROBLEM_UNRECOGNIZED_PARAMETER_VALUE = 104,
    CAUSE_CODE_GENERAL_PROBLEM_UNEXPECTED_PARAMETER_VALUE   = 105,
    CAUSE_CODE_GENERAL_PROBLEM_USER_DATA_SIZE_ERROR         = 106,
    CAUSE_CODE_GENERAL_PROBLEM_OTHER                        = 107,
    /* 108 to 223 reserved, treat as CAUSE_CODE_GENERAL_PROBLEM_OTHER */
    /* 224 to 255 reserved for TIA/EIA-41 extension, otherwise treat as CAUSE_CODE_GENERAL_PROBLEM_OTHER */
} CauseCode;

/* 3GPP2 C.S0015-B, section 4.5, table 4.5-1 */
typedef enum {
    SUBPARAMETER_ID_MESSAGE_ID                      = 0,
    SUBPARAMETER_ID_USER_DATA                       = 1,
    SUBPARAMETER_ID_USER_RESPONSE_CODE              = 2,
    SUBPARAMETER_ID_MESSAGE_CENTER_TIME_STAMP       = 3,
    SUBPARAMETER_ID_VALIDITY_PERIOD_ABSOLUTE        = 4,
    SUBPARAMETER_ID_VALIDITY_PERIOD_RELATIVE        = 5,
    SUBPARAMETER_ID_DEFERRED_DELIVERY_TIME_ABSOLUTE = 6,
    SUBPARAMETER_ID_DEFERRED_DELIVERY_TIME_RELATIVE = 7,
    SUBPARAMETER_ID_PRIORITY_INDICATOR              = 8,
    SUBPARAMETER_ID_PRIVACY_INDICATOR               = 9,
    SUBPARAMETER_ID_REPLY_OPTION                    = 10,
    SUBPARAMETER_ID_NUMBER_OF_MESSAGES              = 11,
    SUBPARAMETER_ID_ALERT_ON_MESSAGE_DELIVERY       = 12,
    SUBPARAMETER_ID_LANGUAGE_INDICATOR              = 13,
    SUBPARAMETER_ID_CALL_BACK_NUMBER                = 14,
    SUBPARAMETER_ID_MESSAGE_DISPLAY_MODE            = 15,
    SUBPARAMETER_ID_MULTIPLE_ENCODING_USER_DATA     = 16,
    SUBPARAMETER_ID_MESSAGE_DEPOSIT_INDEX           = 17,
    SUBPARAMETER_ID_SERVICE_CATEGORY_PROGRAM_DATA   = 18,
    SUBPARAMETER_ID_SERVICE_CATEGORY_PROGRAM_RESULT = 19,
    SUBPARAMETER_ID_MESSAGE_STATUS                  = 20,
    SUBPARAMETER_ID_TP_FAILURE_CAUSE                = 21,
    SUBPARAMETER_ID_ENHANCED_VMN                    = 22,
    SUBPARAMETER_ID_ENHANCED_VMN_ACK                = 23,
} SubparameterId;

/* 3GPP2 C.S0015-B, section 4.5.1, table 4.5.1-1 */
typedef enum {
    TELESERVICE_MESSAGE_TYPE_UNKNOWN                  = 0,
    TELESERVICE_MESSAGE_TYPE_DELIVER                  = 1,
    TELESERVICE_MESSAGE_TYPE_SUBMIT                   = 2,
    TELESERVICE_MESSAGE_TYPE_CANCELLATION             = 3,
    TELESERVICE_MESSAGE_TYPE_DELIVERY_ACKNOWLEDGEMENT = 4,
    TELESERVICE_MESSAGE_TYPE_USER_ACKNOWLEDGEMENT     = 5,
    TELESERVICE_MESSAGE_TYPE_READ_ACKNOWLEDGEMENT     = 6,
} TeleserviceMessageType;

/* C.R1001-G, section 9.1, table 9.1-1 */
typedef enum {
    ENCODING_OCTET                     = 0,
    ENCODING_EXTENDED_PROTOCOL_MESSAGE = 1,
    ENCODING_ASCII_7BIT                = 2,
    ENCODING_IA5                       = 3,
    ENCODING_UNICODE                   = 4,
    ENCODING_SHIFT_JIS                 = 5,
    ENCODING_KOREAN                    = 6,
    ENCODING_LATIN_HEBREW              = 7,
    ENCODING_LATIN                     = 8,
    ENCODING_GSM_7BIT                  = 9,
    ENCODING_GSM_DCS                   = 10,
} Encoding;

static const gchar *
encoding_to_string (Encoding encoding)
{
    static const gchar *encoding_str[] = {
        "octet",
        "extend protocol message",
        "7-bit ASCII",
        "IA5",
        "unicode",
        "shift-j is",
        "korean",
        "latin/hebrew",
        "latin",
        "7-bit GSM",
        "GSM data coding scheme"
    };

    if (encoding >= ENCODING_OCTET && encoding <= ENCODING_GSM_DCS)
        return encoding_str[encoding];

    return "unknown";
}

/*****************************************************************************/
/* Read bits; o_bits < 8; n_bits <= 8
 *
 * Byte 0            Byte 1
 * [7|6|5|4|3|2|1|0] [7|6|5|4|3|2|1|0]
 *
 * o_bits+n_bits <= 16
 *
 */
static guint8
read_bits (const guint8 *bytes,
           guint8 o_bits,
           guint8 n_bits)
{
    guint8 bits_in_first;
    guint8 bits_in_second;

    g_assert (o_bits < 8);
    g_assert (n_bits <= 8);
    g_assert (o_bits + n_bits <= 16);

    /* Read only from the first byte */
    if (o_bits + n_bits <= 8)
        return (bytes[0] >> (8 - o_bits - n_bits)) & ((1 << n_bits) - 1);

    /* Read (8 - o_bits) from the first byte and (n_bits - (8 - o_bits)) from the second byte */
    bits_in_first = 8 - o_bits;
    bits_in_second = n_bits - bits_in_first;
    return (read_bits (&bytes[0], o_bits, bits_in_first) << bits_in_second) | read_bits (&bytes[1], 0, bits_in_second);
}

/*****************************************************************************/
/* Cause code to delivery state */

static MMSmsDeliveryState
cause_code_to_delivery_state (guint8 error_class,
                              guint8 cause_code)
{
    guint delivery_state = 0;

    switch (error_class) {
    case ERROR_CLASS_NO_ERROR:
        return MM_SMS_DELIVERY_STATE_COMPLETED_RECEIVED;
    case ERROR_CLASS_TEMPORARY:
        delivery_state += 0x300;
    case ERROR_CLASS_PERMANENT:
        delivery_state += 0x200;
    default:
        return MM_SMS_DELIVERY_STATE_UNKNOWN;
    }

    /* Fixes for unknown cause codes */

    if (cause_code >= 6 && cause_code <= 31)
        /* 6 to 31 reserved, treat as CAUSE_CODE_NETWORK_PROBLEM_OTHER */
        delivery_state += CAUSE_CODE_NETWORK_PROBLEM_OTHER;
    else if (cause_code >= 40 && cause_code <= 47)
        /* 40 to 47 reserved, treat as CAUSE_CODE_TERMINAL_PROBLEM_OTHER */
        delivery_state += CAUSE_CODE_TERMINAL_PROBLEM_OTHER;
    else if (cause_code >= 48 && cause_code <= 63)
        /* 48 to 63 reserved, treat as CAUSE_CODE_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED */
        delivery_state += CAUSE_CODE_TERMINAL_PROBLEM_SMS_DELIVERY_POSTPONED;
    else if (cause_code >= 67 && cause_code <= 95)
        /* 67 to 95 reserved, treat as CAUSE_CODE_RADIO_INTERFACE_PROBLEM_OTHER */
        delivery_state += CAUSE_CODE_RADIO_INTERFACE_PROBLEM_OTHER;
    else if (cause_code == 101)
        /* 101 reserved */
        delivery_state += CAUSE_CODE_GENERAL_PROBLEM_OTHER;
    else if (cause_code >= 108 && cause_code <= 255)
        /* 108 to 223 reserved, treat as CAUSE_CODE_GENERAL_PROBLEM_OTHER
         * 224 to 255 reserved for TIA/EIA-41 extension, otherwise treat as CAUSE_CODE_GENERAL_PROBLEM_OTHER */
        delivery_state += CAUSE_CODE_GENERAL_PROBLEM_OTHER;
    else
        /* direct relationship */
        delivery_state += cause_code;

    return (MMSmsDeliveryState) delivery_state;
}

/*****************************************************************************/

MMSmsPart *
mm_sms_part_cdma_new_from_pdu (guint index,
                               const gchar *hexpdu,
                               GError **error)
{
    gsize pdu_len;
    guint8 *pdu;
    MMSmsPart *part;

    /* Convert PDU from hex to binary */
    pdu = (guint8 *) mm_utils_hexstr2bin (hexpdu, &pdu_len);
    if (!pdu) {
        g_set_error_literal (error,
                             MM_CORE_ERROR,
                             MM_CORE_ERROR_FAILED,
                             "Couldn't convert CDMA PDU from hex to binary");
        return NULL;
    }

    part = mm_sms_part_cdma_new_from_binary_pdu (index, pdu, pdu_len, error);
    g_free (pdu);

    return part;
}

struct Parameter {
    guint8 parameter_id;
    guint8 parameter_len;
    guint8 parameter_value[];
} __attribute__((packed));

static void
read_teleservice_id (MMSmsPart *sms_part,
                     const struct Parameter *parameter)
{
    guint16 teleservice_id;

    g_assert (parameter->parameter_id == PARAMETER_ID_TELESERVICE_ID);

    if (parameter->parameter_len != 2) {
        mm_dbg ("        invalid teleservice ID length found (%u != 2): ignoring",
                parameter->parameter_len);
        return;
    }

    memcpy (&teleservice_id, &parameter->parameter_value[0], 2);
    teleservice_id = GUINT16_FROM_BE (teleservice_id);

    switch (teleservice_id){
	case MM_SMS_CDMA_TELESERVICE_ID_CMT91:
	case MM_SMS_CDMA_TELESERVICE_ID_WPT:
	case MM_SMS_CDMA_TELESERVICE_ID_WMT:
	case MM_SMS_CDMA_TELESERVICE_ID_VMN:
	case MM_SMS_CDMA_TELESERVICE_ID_WAP:
	case MM_SMS_CDMA_TELESERVICE_ID_WEMT:
	case MM_SMS_CDMA_TELESERVICE_ID_SCPT:
	case MM_SMS_CDMA_TELESERVICE_ID_CATPT:
        break;
    default:
        mm_dbg ("        invalid teleservice ID found (%u): ignoring", teleservice_id);
        return;
    }

    mm_dbg ("        teleservice ID: %s (%u)",
            mm_sms_cdma_teleservice_id_get_string (teleservice_id),
            teleservice_id);

    mm_sms_part_set_cdma_teleservice_id (sms_part,
                                         (MMSmsCdmaTeleserviceId)teleservice_id);
}

static void
read_service_category (MMSmsPart *sms_part,
                       const struct Parameter *parameter)
{
    guint16 service_category;

    g_assert (parameter->parameter_id == PARAMETER_ID_SERVICE_CATEGORY);

    if (parameter->parameter_len != 2) {
        mm_dbg ("        invalid service category length found (%u != 2): ignoring",
                parameter->parameter_len);
        return;
    }

    memcpy (&service_category, &parameter->parameter_value[0], 2);
    service_category = GUINT16_FROM_BE (service_category);

    switch (service_category) {
    case MM_SMS_CDMA_SERVICE_CATEGORY_EMERGENCY_BROADCAST:
    case MM_SMS_CDMA_SERVICE_CATEGORY_ADMINISTRATIVE:
    case MM_SMS_CDMA_SERVICE_CATEGORY_MAINTENANCE:
    case MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_LOCAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_REGIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_NATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_GENERAL_NEWS_INTERNATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_LOCAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_REGIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_NATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_BUSINESS_NEWS_INTERNATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_LOCAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_REGIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_NATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_SPORTS_NEWS_INTERNATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_LOCAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_REGIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_NATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_ENTERTAINMENT_NEWS_INTERNATIONAL:
    case MM_SMS_CDMA_SERVICE_CATEGORY_LOCAL_WEATHER:
    case MM_SMS_CDMA_SERVICE_CATEGORY_TRAFFIC_REPORT:
    case MM_SMS_CDMA_SERVICE_CATEGORY_FLIGHT_SCHEDULES:
    case MM_SMS_CDMA_SERVICE_CATEGORY_RESTAURANTS:
    case MM_SMS_CDMA_SERVICE_CATEGORY_LODGINGS:
    case MM_SMS_CDMA_SERVICE_CATEGORY_RETAIL_DIRECTORY:
    case MM_SMS_CDMA_SERVICE_CATEGORY_ADVERTISEMENTS:
    case MM_SMS_CDMA_SERVICE_CATEGORY_STOCK_QUOTES:
    case MM_SMS_CDMA_SERVICE_CATEGORY_EMPLOYMENT:
    case MM_SMS_CDMA_SERVICE_CATEGORY_HOSPITALS:
    case MM_SMS_CDMA_SERVICE_CATEGORY_TECHNOLOGY_NEWS:
    case MM_SMS_CDMA_SERVICE_CATEGORY_MULTICATEGORY:
    case MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_PRESIDENTIAL_ALERT:
    case MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_EXTREME_THREAT:
    case MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_SEVERE_THREAT:
    case MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY:
    case MM_SMS_CDMA_SERVICE_CATEGORY_CMAS_TEST:
        break;
    default:
        mm_dbg ("        invalid service category found (%u): ignoring", service_category);
        return;
    }

    mm_dbg ("        service category: %s (%u)",
            mm_sms_cdma_service_category_get_string (service_category),
            service_category);

    mm_sms_part_set_cdma_service_category (sms_part,
                                         (MMSmsCdmaServiceCategory)service_category);
}

static guint8
dtmf_to_ascii (guint8 dtmf)
{
    static const gchar dtmf_to_ascii_digits[13] = {
        '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '*', '#' };

    if (dtmf > 0 && dtmf < 13)
        return dtmf_to_ascii_digits[dtmf];

    mm_dbg ("        invalid dtmf digit: %u", dtmf);
    return '\0';
}

static void
read_address (MMSmsPart *sms_part,
              const struct Parameter *parameter)
{
    guint8 digit_mode;
    guint8 number_mode;
    guint8 number_type;
    guint8 numbering_plan;
    guint8 num_fields;
    guint byte_offset = 0;
    guint bit_offset = 0;
    guint i;
    gchar *number = NULL;

#define OFFSETS_UPDATE(n_bits) do { \
        bit_offset += n_bits;       \
        if (bit_offset >= 8) {      \
            bit_offset-=8;          \
            byte_offset++;          \
        }                           \
    } while (0)

#define PARAMETER_SIZE_CHECK(required_size)                             \
    if (parameter->parameter_len < required_size) {                     \
        mm_dbg ("        cannot read address, need at least %u bytes (got %u)", \
                required_size,                                          \
                parameter->parameter_len);                              \
        return;                                                         \
    }

    /* Readability of digit mode and number mode (first 2 bits, i.e. first byte) */
    PARAMETER_SIZE_CHECK (1);

    /* Digit mode */
    digit_mode = read_bits (&parameter->parameter_value[byte_offset], bit_offset, 1);
    OFFSETS_UPDATE (1);
    g_assert (digit_mode <= 1);
    switch (digit_mode) {
    case DIGIT_MODE_DTMF:
        mm_dbg ("        digit mode: dtmf");
        break;
    case DIGIT_MODE_ASCII:
        mm_dbg ("        digit mode: ascii");
        break;
    default:
        g_assert_not_reached ();
    }

    /* Number mode */
    number_mode = read_bits (&parameter->parameter_value[byte_offset], bit_offset, 1);
    OFFSETS_UPDATE (1);
    switch (number_mode) {
    case NUMBER_MODE_DIGIT:
        mm_dbg ("        number mode: digit");
        break;
    case NUMBER_MODE_DATA_NETWORK_ADDRESS:
        mm_dbg ("        number mode: data network address");
        break;
    default:
        g_assert_not_reached ();
    }

    /* Number type */
    if (digit_mode == DIGIT_MODE_ASCII) {
        /* No need for readability check, still in first byte always */
        number_type = read_bits (&parameter->parameter_value[byte_offset], bit_offset, 3);
        OFFSETS_UPDATE (3);
        switch (number_type) {
        case NUMBER_TYPE_UNKNOWN:
            mm_dbg ("        number type: unknown");
            break;
        case NUMBER_TYPE_INTERNATIONAL:
            mm_dbg ("        number type: international");
            break;
        case NUMBER_TYPE_NATIONAL:
            mm_dbg ("        number type: national");
            break;
        case NUMBER_TYPE_NETWORK_SPECIFIC:
            mm_dbg ("        number type: specific");
            break;
        case NUMBER_TYPE_SUBSCRIBER:
            mm_dbg ("        number type: subscriber");
            break;
        case NUMBER_TYPE_ABBREVIATED:
            mm_dbg ("        number type: abbreviated");
            break;
        default:
            mm_dbg ("        number type unknown (%u)", number_type);
            break;
        }
    } else
        number_type = 0xFF;

    /* Numbering plan */
    if (digit_mode == DIGIT_MODE_ASCII && number_mode == NUMBER_MODE_DIGIT) {
        /* Readability of numbering plan; may go to second byte */
        PARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + 4) / 8));
        numbering_plan = read_bits (&parameter->parameter_value[byte_offset], bit_offset, 4);
        OFFSETS_UPDATE (4);
        switch (numbering_plan) {
        case NUMBERING_PLAN_UNKNOWN:
            mm_dbg ("        numbering plan: unknown");
            break;
        case NUMBERING_PLAN_ISDN:
            mm_dbg ("        numbering plan: isdn");
            break;
        case NUMBERING_PLAN_DATA:
            mm_dbg ("        numbering plan: data");
            break;
        case NUMBERING_PLAN_TELEX:
            mm_dbg ("        numbering plan: telex");
            break;
        case NUMBERING_PLAN_PRIVATE:
            mm_dbg ("        numbering plan: private");
            break;
        default:
            mm_dbg ("        numbering plan unknown (%u)", numbering_plan);
            break;
        }
    } else
        numbering_plan = 0xFF;

    /* Readability of num_fields; will go to third byte (((bit_offset + 8) / 8) == 1) */
    PARAMETER_SIZE_CHECK (byte_offset + 2);
    num_fields = read_bits (&parameter->parameter_value[byte_offset], bit_offset, 8);
    OFFSETS_UPDATE (8);
    mm_dbg ("        num fields: %u", num_fields);

    /* Address string */

    if (digit_mode == DIGIT_MODE_DTMF) {
        /* DTMF */
        PARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_fields * 4)) / 8));
        number = g_malloc (num_fields + 1);
        for (i = 0; i < num_fields; i++) {
            number[i] = dtmf_to_ascii (read_bits (&parameter->parameter_value[byte_offset], bit_offset, 4));
            OFFSETS_UPDATE (4);
        }
        number[i] = '\0';
    } else if (number_mode == NUMBER_MODE_DIGIT) {
        /* ASCII
         * TODO: should we expose numbering plan and number type? */
        PARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_fields * 8)) / 8));
        number = g_malloc (num_fields + 1);
        for (i = 0; i < num_fields; i++) {
            number[i] = read_bits (&parameter->parameter_value[byte_offset], bit_offset, 8);
            OFFSETS_UPDATE (8);
        }
        number[i] = '\0';
    } else if (number_type == DATA_NETWORK_ADDRESS_TYPE_INTERNET_EMAIL_ADDRESS) {
        /* Internet e-mail address (ASCII) */
        PARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_fields * 8)) / 8));
        number = g_malloc (num_fields + 1);
        for (i = 0; i < num_fields; i++) {
            number[i] = read_bits (&parameter->parameter_value[byte_offset], bit_offset, 8);
            OFFSETS_UPDATE (8);
        }
        number[i] = '\0';
    } else if (number_type == DATA_NETWORK_ADDRESS_TYPE_INTERNET_PROTOCOL) {
        GString *str;

        /* Binary data network address (most significant first)
         * For now, just print the hex string (e.g. FF:01...) */
        PARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_fields * 8)) / 8));
        str = g_string_sized_new (num_fields * 2);
        for (i = 0; i < num_fields; i++) {
            g_string_append_printf (str, "%.2X", read_bits (&parameter->parameter_value[byte_offset], bit_offset, 8));
            OFFSETS_UPDATE (8);
        }
        number = g_string_free (str, FALSE);
    } else
        mm_dbg ("        data network address number type unknown (%u)", number_type);

    mm_dbg ("        address: %s", number);

    mm_sms_part_set_number (sms_part, number);
    g_free (number);

#undef OFFSETS_UPDATE
#undef PARAMETER_SIZE_CHECK
}

static void
read_bearer_reply_option (MMSmsPart *sms_part,
                          const struct Parameter *parameter)
{
    guint8 sequence;

    g_assert (parameter->parameter_id == PARAMETER_ID_BEARER_REPLY_OPTION);

    if (parameter->parameter_len != 1) {
        mm_dbg ("        invalid bearer reply option length found (%u != 1): ignoring",
                parameter->parameter_len);
        return;
    }

    sequence = read_bits (&parameter->parameter_value[0], 0, 6);
    mm_dbg ("        sequence: %u", sequence);

    mm_sms_part_set_message_reference (sms_part, sequence);
}

static void
read_cause_codes (MMSmsPart *sms_part,
                  const struct Parameter *parameter)
{
    guint8 sequence;
    guint8 error_class;
    guint8 cause_code;
    MMSmsDeliveryState delivery_state;

    g_assert (parameter->parameter_id == PARAMETER_ID_BEARER_REPLY_OPTION);

    if (parameter->parameter_len != 1 && parameter->parameter_len != 2) {
        mm_dbg ("        invalid cause codes length found (%u): ignoring",
                parameter->parameter_len);
        return;
    }

    sequence = read_bits (&parameter->parameter_value[0], 0, 6);
    mm_dbg ("        sequence: %u", sequence);

    error_class = read_bits (&parameter->parameter_value[0], 6, 2);
    mm_dbg ("        error class: %u", error_class);

    if (error_class != ERROR_CLASS_NO_ERROR) {
        if (parameter->parameter_len != 2) {
            mm_dbg ("        invalid cause codes length found (%u != 2): ignoring",
                    parameter->parameter_len);
            return;
        }
        cause_code = parameter->parameter_value[1];
        mm_dbg ("        cause code: %u", cause_code);
    } else
        cause_code = 0;

    delivery_state = cause_code_to_delivery_state (error_class, cause_code);
    mm_dbg ("        delivery state: %s", mm_sms_delivery_state_get_string (delivery_state));

    mm_sms_part_set_message_reference (sms_part, sequence);
    mm_sms_part_set_delivery_state (sms_part, delivery_state);
}

static void
read_bearer_data_message_identifier (MMSmsPart *sms_part,
                                     const struct Parameter *subparameter)
{
    guint8 message_type;
    guint16 message_id;
    guint8 header_ind;

    g_assert (subparameter->parameter_id == SUBPARAMETER_ID_MESSAGE_ID);

    if (subparameter->parameter_len != 3) {
        mm_dbg ("        invalid message identifier length found (%u): ignoring",
                subparameter->parameter_len);
        return;
    }

    message_type = read_bits (&subparameter->parameter_value[0], 0, 4);
    switch (message_type) {
    case TELESERVICE_MESSAGE_TYPE_UNKNOWN:
        mm_dbg ("            message type: unknown");
        break;
    case TELESERVICE_MESSAGE_TYPE_DELIVER:
        mm_dbg ("            message type: deliver");
        mm_sms_part_set_pdu_type (sms_part, MM_SMS_PDU_TYPE_CDMA_DELIVER);
        break;
    case TELESERVICE_MESSAGE_TYPE_SUBMIT:
        mm_dbg ("            message type: submit");
        mm_sms_part_set_pdu_type (sms_part, MM_SMS_PDU_TYPE_CDMA_SUBMIT);
        break;
    case TELESERVICE_MESSAGE_TYPE_CANCELLATION:
        mm_dbg ("            message type: cancellation");
        mm_sms_part_set_pdu_type (sms_part, MM_SMS_PDU_TYPE_CDMA_CANCELLATION);
        break;
    case TELESERVICE_MESSAGE_TYPE_DELIVERY_ACKNOWLEDGEMENT:
        mm_dbg ("            message type: delivery acknowledgement");
        mm_sms_part_set_pdu_type (sms_part, MM_SMS_PDU_TYPE_CDMA_DELIVERY_ACKNOWLEDGEMENT);
        break;
    case TELESERVICE_MESSAGE_TYPE_USER_ACKNOWLEDGEMENT:
        mm_dbg ("            message type: user acknowledgement");
        mm_sms_part_set_pdu_type (sms_part, MM_SMS_PDU_TYPE_CDMA_USER_ACKNOWLEDGEMENT);
        break;
    case TELESERVICE_MESSAGE_TYPE_READ_ACKNOWLEDGEMENT:
        mm_dbg ("            message type: read acknowledgement");
        mm_sms_part_set_pdu_type (sms_part, MM_SMS_PDU_TYPE_CDMA_READ_ACKNOWLEDGEMENT);
        break;
    default:
        mm_dbg ("            message type unknown (%u)", message_type);
        break;
    }

    message_id = ((read_bits (&subparameter->parameter_value[0], 4, 8) << 8) |
                  (read_bits (&subparameter->parameter_value[1], 4, 8)));
    message_id = GUINT16_FROM_BE (message_id);
    mm_dbg ("            message id: %u", (guint) message_id);

    header_ind = read_bits (&subparameter->parameter_value[2], 4, 1);
    mm_dbg ("            header indicator: %u", header_ind);
}

static void
read_bearer_data_user_data (MMSmsPart *sms_part,
                            const struct Parameter *subparameter)
{
    guint8 message_encoding;
    guint8 message_type = 0;
    guint8 num_fields;
    guint byte_offset = 0;
    guint bit_offset = 0;

#define OFFSETS_UPDATE(n_bits) do { \
        bit_offset += n_bits;       \
        if (bit_offset >= 8) {      \
            bit_offset-=8;          \
            byte_offset++;          \
        }                           \
    } while (0)

#define SUBPARAMETER_SIZE_CHECK(required_size)                             \
    if (subparameter->parameter_len < required_size) {                  \
        mm_dbg ("        cannot read user data, need at least %u bytes (got %u)", \
                required_size,                                          \
                subparameter->parameter_len);                           \
        return;                                                         \
    }

    g_assert (subparameter->parameter_id == SUBPARAMETER_ID_USER_DATA);

    /* Message encoding */
    SUBPARAMETER_SIZE_CHECK (1);
    message_encoding = read_bits (&subparameter->parameter_value[byte_offset], bit_offset, 5);
    OFFSETS_UPDATE (5);
    mm_dbg ("            message encoding: %s", encoding_to_string (message_encoding));

    /* Message type, only if extended protocol message */
    if (message_encoding == ENCODING_EXTENDED_PROTOCOL_MESSAGE) {
        SUBPARAMETER_SIZE_CHECK (2);
        message_type = read_bits (&subparameter->parameter_value[byte_offset], bit_offset, 8);
        OFFSETS_UPDATE (8);
        mm_dbg ("            message type: %u", message_type);
    }

    /* Number of fields */
    SUBPARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + 8) / 8));
    num_fields = read_bits (&subparameter->parameter_value[byte_offset], bit_offset, 8);
    OFFSETS_UPDATE (8);
    mm_dbg ("            num fields: %u", num_fields);

    /* Now, process actual text or data */
    switch (message_encoding) {
    case ENCODING_OCTET: {
        GByteArray *data;
        guint i;

        SUBPARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_fields * 8)) / 8));

        data = g_byte_array_sized_new (num_fields);
        g_byte_array_set_size (data, num_fields);
        for (i = 0; i < num_fields; i++) {
            data->data[i] = read_bits (&subparameter->parameter_value[byte_offset], bit_offset, 8);
            OFFSETS_UPDATE (8);
        }

        mm_dbg ("            data: (%u bytes)", num_fields);
        mm_sms_part_take_data (sms_part, data);
        break;
    }

    case ENCODING_ASCII_7BIT: {
        gchar *text;
        guint i;

        SUBPARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_fields * 7)) / 8));

        text = g_malloc (num_fields + 1);
        for (i = 0; i < num_fields; i++) {
            text[i] = read_bits (&subparameter->parameter_value[byte_offset], bit_offset, 7);
            OFFSETS_UPDATE (7);
        }
        text[i] = '\0';

        mm_dbg ("            text: '%s'", text);
        mm_sms_part_take_text (sms_part, text);
        break;
    }

    case ENCODING_LATIN: {
        gchar *latin;
        gchar *text;
        guint i;

        SUBPARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_fields * 8)) / 8));

        latin = g_malloc (num_fields + 1);
        for (i = 0; i < num_fields; i++) {
            latin[i] = read_bits (&subparameter->parameter_value[byte_offset], bit_offset, 8);
            OFFSETS_UPDATE (8);
        }
        latin[i] = '\0';

        text = g_convert (latin, -1, "UTF-8", "ISO−8859−1", NULL, NULL, NULL);
        if (!text) {
            mm_dbg ("            text/data: ignored (latin to UTF-8 conversion error)");
        } else {
            mm_dbg ("            text: '%s'", text);
            mm_sms_part_take_text (sms_part, text);
        }

        g_free (latin);
        break;
    }

    case ENCODING_UNICODE: {
        gchar *utf16;
        gchar *text;
        guint i;
        guint num_bytes;

        /* 2 bytes per field! */
        num_bytes = num_fields * 2;

        SUBPARAMETER_SIZE_CHECK (byte_offset + 1 + ((bit_offset + (num_bytes * 8)) / 8));

        utf16 = g_malloc (num_bytes);
        for (i = 0; i < num_bytes; i++) {
            utf16[i] = read_bits (&subparameter->parameter_value[byte_offset], bit_offset, 8);
            OFFSETS_UPDATE (8);
        }

        text = g_convert (utf16, num_bytes, "UTF-8", "UCS-2BE", NULL, NULL, NULL);
        if (!text) {
            mm_dbg ("            text/data: ignored (UTF-16 to UTF-8 conversion error)");
        } else {
            mm_dbg ("            text: '%s'", text);
            mm_sms_part_take_text (sms_part, text);
        }

        g_free (utf16);
        break;
    }

    default:
        mm_dbg ("            text/data: ignored (unsupported encoding)");
    }

#undef OFFSETS_UPDATE
#undef SUBPARAMETER_SIZE_CHECK
}

static void
read_bearer_data (MMSmsPart *sms_part,
                  const struct Parameter *parameter)
{
    guint offset;

#define PARAMETER_SIZE_CHECK(required_size)                             \
    if (parameter->parameter_len < required_size) {                     \
        mm_dbg ("        cannot read bearer data, need at least %u bytes (got %u)", \
                required_size,                                          \
                parameter->parameter_len);                              \
        return;                                                         \
    }

    offset = 0;
    while (offset < parameter->parameter_len) {
        const struct Parameter *subparameter;

        PARAMETER_SIZE_CHECK (offset + 2);
        subparameter = (const struct Parameter *)&parameter->parameter_value[offset];
        offset += 2;

        PARAMETER_SIZE_CHECK (offset + subparameter->parameter_len);
        offset += subparameter->parameter_len;

        switch (subparameter->parameter_id) {
        case SUBPARAMETER_ID_MESSAGE_ID:
            mm_dbg ("        reading message ID...");
            read_bearer_data_message_identifier (sms_part, subparameter);
            break;
        case SUBPARAMETER_ID_USER_DATA:
            mm_dbg ("        reading user data...");
            read_bearer_data_user_data (sms_part, subparameter);
            break;
        case SUBPARAMETER_ID_USER_RESPONSE_CODE:
            mm_dbg ("        skipping user response code...");
            break;
        case SUBPARAMETER_ID_MESSAGE_CENTER_TIME_STAMP:
            mm_dbg ("        skipping message center timestamp...");
            break;
        case SUBPARAMETER_ID_VALIDITY_PERIOD_ABSOLUTE:
            mm_dbg ("        skipping absolute validity period...");
            break;
        case SUBPARAMETER_ID_VALIDITY_PERIOD_RELATIVE:
            mm_dbg ("        skipping relative validity period...");
            break;
        case SUBPARAMETER_ID_DEFERRED_DELIVERY_TIME_ABSOLUTE:
            mm_dbg ("        skipping absolute deferred delivery time...");
            break;
        case SUBPARAMETER_ID_DEFERRED_DELIVERY_TIME_RELATIVE:
            mm_dbg ("        skipping relative deferred delivery time...");
            break;
        case SUBPARAMETER_ID_PRIORITY_INDICATOR:
            mm_dbg ("        skipping priority indicator...");
            break;
        case SUBPARAMETER_ID_PRIVACY_INDICATOR:
            mm_dbg ("        skipping privacy indicator...");
            break;
        case SUBPARAMETER_ID_REPLY_OPTION:
            mm_dbg ("        skipping reply option...");
            break;
        case SUBPARAMETER_ID_NUMBER_OF_MESSAGES:
            mm_dbg ("        skipping number of messages...");
            break;
        case SUBPARAMETER_ID_ALERT_ON_MESSAGE_DELIVERY:
            mm_dbg ("        skipping alert on message delivery...");
            break;
        case SUBPARAMETER_ID_LANGUAGE_INDICATOR:
            mm_dbg ("        skipping language indicator...");
            break;
        case SUBPARAMETER_ID_CALL_BACK_NUMBER:
            mm_dbg ("        skipping call back number...");
            break;
        case SUBPARAMETER_ID_MESSAGE_DISPLAY_MODE:
            mm_dbg ("        skipping message display mode...");
            break;
        case SUBPARAMETER_ID_MULTIPLE_ENCODING_USER_DATA:
            mm_dbg ("        skipping multiple encoding user data...");
            break;
        case SUBPARAMETER_ID_MESSAGE_DEPOSIT_INDEX:
            mm_dbg ("        skipping message deposit index...");
            break;
        case SUBPARAMETER_ID_SERVICE_CATEGORY_PROGRAM_DATA:
            mm_dbg ("        skipping service category program data...");
            break;
        case SUBPARAMETER_ID_SERVICE_CATEGORY_PROGRAM_RESULT:
            mm_dbg ("        skipping service category program result...");
            break;
        case SUBPARAMETER_ID_MESSAGE_STATUS:
            mm_dbg ("        skipping message status...");
            break;
        case SUBPARAMETER_ID_TP_FAILURE_CAUSE:
            mm_dbg ("        skipping TP failure case...");
            break;
        case SUBPARAMETER_ID_ENHANCED_VMN:
            mm_dbg ("        skipping enhanced vmn...");
            break;
        case SUBPARAMETER_ID_ENHANCED_VMN_ACK:
            mm_dbg ("        skipping enhanced vmn ack...");
            break;
        default:
            mm_dbg ("    unknown subparameter found: '%u' (ignoring)",
                    subparameter->parameter_id);
            break;
        }
    }

#undef PARAMETER_SIZE_CHECK
}

MMSmsPart *
mm_sms_part_cdma_new_from_binary_pdu (guint index,
                                      const guint8 *pdu,
                                      gsize pdu_len,
                                      GError **error)
{
    MMSmsPart *sms_part;
    guint offset;
    guint message_type;

    /* Create the new MMSmsPart */
    sms_part = mm_sms_part_new (index, MM_SMS_PDU_TYPE_UNKNOWN);

    if (index != SMS_PART_INVALID_INDEX)
        mm_dbg ("Parsing CDMA PDU (%u)...", index);
    else
        mm_dbg ("Parsing CDMA PDU...");

#define PDU_SIZE_CHECK(required_size, check_descr_str)                 \
    if (pdu_len < required_size) {                                     \
        g_set_error (error,                                            \
                     MM_CORE_ERROR,                                    \
                     MM_CORE_ERROR_FAILED,                             \
                     "CDMA PDU too short, %s: %" G_GSIZE_FORMAT " < %u",    \
                     check_descr_str,                                  \
                     pdu_len,                                          \
                     required_size);                                   \
        mm_sms_part_free (sms_part);                                   \
        return NULL;                                                   \
    }

    offset = 0;

    /* First byte: SMS message type */
    PDU_SIZE_CHECK (offset + 1, "cannot read SMS message type");
    message_type = pdu[offset++];
    switch (message_type) {
    case MESSAGE_TYPE_POINT_TO_POINT:
    case MESSAGE_TYPE_BROADCAST:
    case MESSAGE_TYPE_ACKNOWLEDGE:
        break;
    default:
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Invalid SMS message type (%u)",
                     message_type);
        mm_sms_part_free (sms_part);
        return NULL;
    }

    /* Now walk parameters one by one */
    while (offset < pdu_len) {
        const struct Parameter *parameter;

        PDU_SIZE_CHECK (offset + 2, "cannot read parameter header");
        parameter = (const struct Parameter *)&pdu[offset];
        offset += 2;

        PDU_SIZE_CHECK (offset + parameter->parameter_len, "cannot read parameter value");
        offset += parameter->parameter_len;

        switch (parameter->parameter_id) {
        case PARAMETER_ID_TELESERVICE_ID:
            mm_dbg ("    reading teleservice ID...");
            read_teleservice_id (sms_part, parameter);
            break;
        case PARAMETER_ID_SERVICE_CATEGORY:
            mm_dbg ("    reading service category...");
            read_service_category (sms_part, parameter);
            break;
        case PARAMETER_ID_ORIGINATING_ADDRESS:
            mm_dbg ("    reading originating address...");
            if (mm_sms_part_get_number (sms_part))
                mm_dbg ("        cannot read originating address; an address field was already read");
            else
                read_address (sms_part, parameter);
            break;
        case PARAMETER_ID_ORIGINATING_SUBADDRESS:
            mm_dbg ("    skipping originating subaddress...");
            break;
        case PARAMETER_ID_DESTINATION_ADDRESS:
            mm_dbg ("    reading destination address...");
            if (mm_sms_part_get_number (sms_part))
                mm_dbg ("        cannot read destination address; an address field was already read");
            else
                read_address (sms_part, parameter);
            break;
        case PARAMETER_ID_DESTINATION_SUBADDRESS:
            mm_dbg ("    skipping destination subaddress...");
            break;
        case PARAMETER_ID_BEARER_REPLY_OPTION:
            mm_dbg ("    reading bearer reply option...");
            read_bearer_reply_option (sms_part, parameter);
            break;
        case PARAMETER_ID_CAUSE_CODES:
            mm_dbg ("    reading cause codes...");
            read_cause_codes (sms_part, parameter);
            break;
        case PARAMETER_ID_BEARER_DATA:
            mm_dbg ("    reading bearer data...");
            read_bearer_data (sms_part, parameter);
            break;
        default:
            mm_dbg ("    unknown parameter found: '%u' (ignoring)",
                    parameter->parameter_id);
            break;
        }
    }

    /* Check mandatory parameters */
    switch (message_type) {
    case MESSAGE_TYPE_POINT_TO_POINT:
        if (mm_sms_part_get_cdma_teleservice_id (sms_part) == MM_SMS_CDMA_TELESERVICE_ID_UNKNOWN)
            mm_dbg ("    mandatory parameter missing: teleservice ID not found or invalid in point-to-point message");
        break;
    case MESSAGE_TYPE_BROADCAST:
        if (mm_sms_part_get_cdma_service_category (sms_part) == MM_SMS_CDMA_SERVICE_CATEGORY_UNKNOWN)
            mm_dbg ("    mandatory parameter missing: service category not found or invalid in broadcast message");
        break;
    case MESSAGE_TYPE_ACKNOWLEDGE:
        if (mm_sms_part_get_message_reference (sms_part) == 0)
            mm_dbg ("    mandatory parameter missing: cause codes not found or invalid in acknowledge message");
        break;
    }

#undef PDU_SIZE_CHECK

    return sms_part;
}

/*****************************************************************************/
/* Write bits; o_bits < 8; n_bits <= 8
 *
 * Byte 0            Byte 1
 * [7|6|5|4|3|2|1|0] [7|6|5|4|3|2|1|0]
 *
 * o_bits+n_bits <= 16
 *
 * NOTE! The bits being set should be 0 initially.
 */
static void
write_bits (guint8 *bytes,
            guint8 o_bits,
            guint8 n_bits,
            guint8 bits)
{
    guint8 bits_in_first;
    guint8 bits_in_second;

    g_assert (o_bits < 8);
    g_assert (n_bits <= 8);
    g_assert (o_bits + n_bits <= 16);

    /* Write only in the first byte */
    if (o_bits + n_bits <= 8) {
        bytes[0] |= (bits & ((1 << n_bits) - 1)) << (8 - o_bits - n_bits);
        return;
    }

    /* Write (8 - o_bits) in the first byte and (n_bits - (8 - o_bits)) in the second byte */
    bits_in_first = 8 - o_bits;
    bits_in_second = n_bits - bits_in_first;

    write_bits (&bytes[0], o_bits, bits_in_first, (bits >> bits_in_second));
    write_bits (&bytes[1], 0, bits_in_second, bits);
}

/*****************************************************************************/

static guint8
dtmf_from_ascii (guint8 ascii)
{
    if (ascii >= '1' && ascii <= '9')
        return ascii - '0';
    if (ascii == '0')
        return 10;
    if (ascii == '*')
        return 11;
    if (ascii == '#')
        return 12;

    mm_dbg ("        invalid ascii digit in dtmf conversion: %c", ascii);
    return 0;
}

static gboolean
write_teleservice_id (MMSmsPart *part,
                      guint8 *pdu,
                      guint *absolute_offset,
                      GError **error)
{
    guint16 aux16;

    mm_dbg ("    writing teleservice ID...");

    if (mm_sms_part_get_cdma_teleservice_id (part) != MM_SMS_CDMA_TELESERVICE_ID_WMT) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Teleservice '%s' not supported",
                     mm_sms_cdma_teleservice_id_get_string (
                         mm_sms_part_get_cdma_teleservice_id (part)));
        return FALSE;
    }

    mm_dbg ("        teleservice ID: %s (%u)",
            mm_sms_cdma_teleservice_id_get_string (MM_SMS_CDMA_TELESERVICE_ID_WMT),
            MM_SMS_CDMA_TELESERVICE_ID_WMT);

    /* Teleservice ID: WMT always */
    pdu[0] = PARAMETER_ID_TELESERVICE_ID;
    pdu[1] = 2; /* parameter_len, always 2 */
    aux16 = GUINT16_TO_BE (MM_SMS_CDMA_TELESERVICE_ID_WMT);
    memcpy (&pdu[2], &aux16, 2);

    *absolute_offset += 4;
    return TRUE;
}

static gboolean
write_destination_address (MMSmsPart *part,
                           guint8 *pdu,
                           guint *absolute_offset,
                           GError **error)
{
    const gchar *number;
    guint bit_offset;
    guint byte_offset;
    guint n_digits;
    guint i;
    DigitMode digit_mode;
    NumberMode number_mode;

    mm_dbg ("    writing destination address...");

#define OFFSETS_UPDATE(n_bits) do { \
        bit_offset += n_bits;       \
        if (bit_offset >= 8) {      \
            bit_offset-=8;          \
            byte_offset++;          \
        }                           \
    } while (0)

    number = mm_sms_part_get_number (part);
    n_digits = strlen (number);

    pdu[0] = PARAMETER_ID_DESTINATION_ADDRESS;
    /* Write parameter length at the end */

    byte_offset = 2;
    bit_offset = 0;

    /* Digit mode: ASCII for International numbers, otherwise DTMF */
    digit_mode = g_str_has_prefix (number, "+") ? DIGIT_MODE_ASCII : DIGIT_MODE_DTMF;
    mm_dbg ("        digit mode: %s", digit_mode == DIGIT_MODE_DTMF ? "dtmf" : "ascii");
    write_bits (&pdu[byte_offset], bit_offset, 1, digit_mode);
    OFFSETS_UPDATE (1);

    /* Number mode: DIGIT always */
    number_mode = NUMBER_MODE_DIGIT;
    mm_dbg ("        number mode: digit");
    write_bits (&pdu[byte_offset], bit_offset, 1, number_mode);
    OFFSETS_UPDATE (1);

    /* Number type and numbering plan only needed in ASCII digit mode */
    if (digit_mode == DIGIT_MODE_ASCII) {
        g_assert (g_str_has_prefix (number, "+"));
        mm_dbg ("        number type: international");
        write_bits (&pdu[byte_offset], bit_offset, 3, NUMBER_TYPE_INTERNATIONAL);
        OFFSETS_UPDATE (3);

        g_assert (number_mode == NUMBER_MODE_DIGIT);

        mm_dbg ("        numbering plan: unknown");
        write_bits (&pdu[byte_offset], bit_offset, 4, NUMBERING_PLAN_UNKNOWN);
        OFFSETS_UPDATE (4);
    }

    /* Number of fields */
    if (n_digits > 256) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Number too long (max 256 digits, %u given)",
                     n_digits);
        return FALSE;
    }
    mm_dbg ("        num fields: %u", n_digits);
    write_bits (&pdu[byte_offset], bit_offset, 8, n_digits);
    OFFSETS_UPDATE (8);

    /* Actual DTMF encoded number */
    mm_dbg ("        address: %s", number);
    if (digit_mode == DIGIT_MODE_DTMF) {
        /* DTMF! Address given with 4-bit characters */
        for (i = 0; i < n_digits; i++) {
            guint8 dtmf;

            dtmf = dtmf_from_ascii (number[i]);
            if (!dtmf) {
                g_set_error (error,
                             MM_CORE_ERROR,
                             MM_CORE_ERROR_UNSUPPORTED,
                             "Unsupported character in number: '%c'. Cannot convert to DTMF",
                             number[i]);
            return FALSE;
            }
            write_bits (&pdu[byte_offset], bit_offset, 4, dtmf);
            OFFSETS_UPDATE (4);
        }
    } else {
        /* ASCII! Address given with 8-bit characters */
        for (i = 0; i < n_digits; i++) {
            write_bits (&pdu[byte_offset], bit_offset, 8, number[i]);
            OFFSETS_UPDATE (8);
        }
    }

#undef OFFSETS_UPDATE

    /* Write parameter length (remove header length to offset) */
    byte_offset += !!bit_offset - 2;
    if (byte_offset > 256) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Number too long (max 256 bytes, %u given)",
                     byte_offset);
        return FALSE;
    }
    pdu[1] = byte_offset;

    *absolute_offset += (2 + pdu[1]);
    return TRUE;
}

static gboolean
write_bearer_data_message_identifier (MMSmsPart *part,
                                      guint8 *pdu,
                                      guint *parameter_offset,
                                      GError **error)
{
    pdu[0] = SUBPARAMETER_ID_MESSAGE_ID;
    pdu[1] = 3; /* subparameter_len, always 3 */

    mm_dbg ("        writing message identifier: submit");

    /* Message type */
    write_bits (&pdu[2], 0, 4, TELESERVICE_MESSAGE_TYPE_SUBMIT);

    /* Skip adding a message id; assume it's filled in by device */

    /* And no need for a header ind value, always false */

    *parameter_offset += 5;
    return TRUE;
}

static void
decide_best_encoding (const gchar *text,
                      GByteArray **out,
                      guint *num_fields,
                      guint *num_bits_per_field,
                      Encoding *encoding)
{
    guint latin_unsupported = 0;
    guint ascii_unsupported = 0;
    guint i;
    guint len;

    len = strlen (text);

    /* Check if we can do ASCII-7 */
    for (i = 0; i < len; i++) {
        if (text[i] & 0x80) {
            ascii_unsupported++;
            break;
        }
    }

    /* If ASCII-7 already supported, done we are */
    if (!ascii_unsupported) {
        *out = g_byte_array_sized_new (len);
        g_byte_array_append (*out, (const guint8 *)text, len);
        *num_fields = len;
        *num_bits_per_field = 7;
        *encoding = ENCODING_ASCII_7BIT;
        return;
    }

    /* Check if we can do Latin encoding */
    mm_charset_get_encoded_len (text,
                                MM_MODEM_CHARSET_8859_1,
                                &latin_unsupported);
    if (!latin_unsupported) {
        *out = g_byte_array_sized_new (len);
        mm_modem_charset_byte_array_append (*out,
                                            text,
                                            FALSE,
                                            MM_MODEM_CHARSET_8859_1);
        *num_fields = (*out)->len;
        *num_bits_per_field = 8;
        *encoding = ENCODING_LATIN;
        return;
    }

    /* If no Latin and no ASCII, default to UTF-16 */
    *out = g_byte_array_sized_new (len * 2);
    mm_modem_charset_byte_array_append (*out,
                                        text,
                                        FALSE,
                                        MM_MODEM_CHARSET_UCS2);
    *num_fields = (*out)->len / 2;
    *num_bits_per_field = 16;
    *encoding = ENCODING_UNICODE;
}

static gboolean
write_bearer_data_user_data (MMSmsPart *part,
                             guint8 *pdu,
                             guint *parameter_offset,
                             GError **error)
{
    const gchar *text;
    const GByteArray *data;
    guint bit_offset = 0;
    guint byte_offset = 0;
    guint num_fields;
    guint num_bits_per_field;
    guint i;
    Encoding encoding;
    GByteArray *converted = NULL;
    const GByteArray *aux;
    guint num_bits_per_iter;

    mm_dbg ("        writing user data...");

#define OFFSETS_UPDATE(n_bits) do { \
        bit_offset += n_bits;       \
        if (bit_offset >= 8) {      \
            bit_offset-=8;          \
            byte_offset++;          \
        }                           \
    } while (0)

    text = mm_sms_part_get_text (part);
    data = mm_sms_part_get_data (part);
    g_assert (text || data);
    g_assert (!(!text && !data));

    pdu[0] = SUBPARAMETER_ID_USER_DATA;
    /* Write parameter length at the end */
    byte_offset = 2;
    bit_offset = 0;

    /* Text or Data */
    if (text) {
        decide_best_encoding (text,
                              &converted,
                              &num_fields,
                              &num_bits_per_field,
                              &encoding);
        aux = (const GByteArray *)converted;
    } else {
        aux = data;
        num_fields = data->len;
        num_bits_per_field = 8;
        encoding = ENCODING_OCTET;
    }

    /* Message encoding*/
    mm_dbg ("            message encoding: %s", encoding_to_string (encoding));
    write_bits (&pdu[byte_offset], bit_offset, 5, encoding);
    OFFSETS_UPDATE (5);

    /* Number of fields */
    if (num_fields > 256) {
        if (converted)
            g_byte_array_unref (converted);
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Data too long (max 256 fields, %u given)",
                     num_fields);
        return FALSE;
    }
    mm_dbg ("            num fields: %u", num_fields);
    write_bits (&pdu[byte_offset], bit_offset, 8, num_fields);
    OFFSETS_UPDATE (8);

    /* For ASCII-7, write 7 bits in each iteration; for the remaining ones
     * go byte per byte */
    if (text)
        mm_dbg ("            text: '%s'", text);
    else
        mm_dbg ("            data: (%u bytes)", num_fields);
    num_bits_per_iter = num_bits_per_field < 8 ? num_bits_per_field : 8;
    for (i = 0; i < aux->len; i++) {
        write_bits (&pdu[byte_offset], bit_offset, num_bits_per_iter, aux->data[i]);
        OFFSETS_UPDATE (num_bits_per_iter);
    }

    if (converted)
        g_byte_array_unref (converted);

#undef OFFSETS_UPDATE

    /* Write subparameter length (remove header length to offset) */
    byte_offset += !!bit_offset - 2;
    if (byte_offset > 256) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Data or Text too long (max 256 bytes, %u given)",
                     byte_offset);
        return FALSE;
    }
    pdu[1] = byte_offset;

    *parameter_offset += (2 + pdu[1]);
    return TRUE;
}

static gboolean
write_bearer_data (MMSmsPart *part,
                   guint8 *pdu,
                   guint *absolute_offset,
                   GError **error)
{
    GError *inner_error = NULL;
    guint offset = 0;

    mm_dbg ("    writing bearer data...");

    pdu[0] = PARAMETER_ID_BEARER_DATA;
    /* Write parameter length at the end */

    offset = 2;
    if (!write_bearer_data_message_identifier (part, &pdu[offset], &offset, &inner_error))
        mm_dbg ("Error writing message identifier: %s", inner_error->message);
    else if (!write_bearer_data_user_data (part, &pdu[offset], &offset, &inner_error))
        mm_dbg ("Error writing user data: %s", inner_error->message);

    if (inner_error) {
        g_propagate_error (error, inner_error);
        g_prefix_error (error, "Error writing bearer data: ");
        return FALSE;
    }

    /* Write parameter length (remove header length to offset) */
    offset -= 2;
    if (offset > 256) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Bearer data too long (max 256 bytes, %u given)",
                     offset);
        return FALSE;
    }
    pdu[1] = offset;

    *absolute_offset += (2 + pdu[1]);
    return TRUE;
}

guint8 *
mm_sms_part_cdma_get_submit_pdu (MMSmsPart *part,
                                 guint *out_pdulen,
                                 GError **error)
{
    GError *inner_error = NULL;
    guint offset = 0;
    guint8 *pdu;

    g_return_val_if_fail (mm_sms_part_get_number (part) != NULL, NULL);
    g_return_val_if_fail (mm_sms_part_get_text (part) != NULL || mm_sms_part_get_data (part) != NULL, NULL);

    if (mm_sms_part_get_pdu_type (part) != MM_SMS_PDU_TYPE_CDMA_SUBMIT) {
        g_set_error (error,
                     MM_MESSAGE_ERROR,
                     MM_MESSAGE_ERROR_INVALID_PDU_PARAMETER,
                     "Invalid PDU type to generate a 'submit' PDU: '%s'",
                     mm_sms_pdu_type_get_string (mm_sms_part_get_pdu_type (part)));
        return NULL;
    }

    mm_dbg ("Creating PDU for part...");

    /* Current max size estimations:
     *  Message type: 1 byte
     *  Teleservice ID: 5 bytes
     *  Destination address: 2 + 256 bytes
     *  Bearer data: 2 + 256 bytes
     */
    pdu = g_malloc0 (1024);

    /* First byte: SMS message type */
    pdu[offset++] = MESSAGE_TYPE_POINT_TO_POINT;

    if (!write_teleservice_id (part, &pdu[offset], &offset, &inner_error))
        mm_dbg ("Error writing Teleservice ID: %s", inner_error->message);
    else if (!write_destination_address (part, &pdu[offset], &offset, &inner_error))
        mm_dbg ("Error writing destination address: %s", inner_error->message);
    else if (!write_bearer_data (part, &pdu[offset], &offset, &inner_error))
        mm_dbg ("Error writing bearer data: %s", inner_error->message);

    if (inner_error) {
        g_propagate_error (error, inner_error);
        g_prefix_error (error, "Cannot create CDMA SMS part: ");
        g_free (pdu);
        return NULL;
    }

    *out_pdulen = offset;
    return pdu;
}
