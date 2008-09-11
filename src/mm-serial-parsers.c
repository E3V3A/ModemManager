/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <string.h>
#include <stdlib.h>

#include "mm-serial-parsers.h"
#include "mm-errors.h"

/* Clean up the response by removing control characters like <CR><LF> etc */
static void
response_clean (GString *response)
{
    char *s;

    /* Ends with '<CR><LF>' */
    s = response->str + response->len - 1;
    if (*s == '\n' && *(--s) == '\r')
        g_string_truncate (response, response->len - 2);

    /* Starts with '<CR><LF>' */
    s = response->str;
    if (*s == '\r' && *(++s) == '\n')
        g_string_erase (response, 0, 2);
}


static gboolean
remove_eval_cb (const GMatchInfo *match_info,
                GString *result,
                gpointer user_data)
{
    int *result_len = (int *) user_data;
    int start;
    int end;

    if (g_match_info_fetch_pos  (match_info, 0, &start, &end))
        *result_len -= (end - start);

    return TRUE;
}

static void
remove_matches (GRegex *r, GString *string)
{
    char *str;
    int result_len = string->len;

    str = g_regex_replace_eval (r, string->str, string->len, 0, 0,
                                remove_eval_cb, &result_len, NULL);

    g_string_truncate (string, 0);
    g_string_append_len (string, str, result_len);
    g_free (str);
}

/* FIXME: V0 parser is not finished */
#if 0
typedef struct {
    GRegex *generic_response;
    GRegex *detailed_error;
} MMSerialParserV0;

gpointer
mm_serial_parser_v0_new (void)
{
    MMSerialParserV0 *parser;
    GRegexCompileFlags flags = G_REGEX_DOLLAR_ENDONLY | G_REGEX_RAW | G_REGEX_OPTIMIZE;

    parser = g_slice_new (MMSerialParserV0);

    parser->generic_response = g_regex_new ("(\\d)\\r%", flags, 0, NULL);
    parser->detailed_error = g_regex_new ("+CME ERROR: (\\d+)\\r\\n$", flags, 0, NULL);

    return parser;
}

gboolean
mm_serial_parser_v0_parse (gpointer parser,
                           GString *response,
                           GError **error)
{
    MMSerialParserV0 *parser = (MMSerialParserV0 *) data;
    GMatchInfo *match_info;
    char *str;
    int code;
    gboolean found;

    found = g_regex_match_full (parser->generic_response, response->str, response->len, 0, 0, &match_info, NULL);
    if (found) {
        str = g_match_info_fetch (match_info, 1);
        if (str) {
            code = atoi (str);
            g_free (str);
        }

        g_match_info_free (match_info);

        return TRUE;
    }

    found = g_regex_match_full (parser->detailed_error, response->str, response->len, 0, 0, &match_info, NULL);
    if (found) {
        str = g_match_info_fetch (match_info, 1);
        if (str) {
            code = atoi (str);
            g_free (str);
        } else
            code = MM_MOBILE_ERROR_UNKNOWN;

        g_match_info_free (match_info);

        g_debug ("Got error code %d: %s", code, msg);
        g_set_error (error, MM_MOBILE_ERROR, code, "%s", msg);

        return TRUE;
    }

    return FALSE;
}

void
mm_serial_parser_v0_destroy (gpointer parser)
{
    MMSerialParserV0 *parser = (MMSerialParserV0 *) data;

    g_regex_unref (parser->generic_response);
    g_regex_unref (parser->detailed_error);

    g_slice_free (MMSerialParserV0, data);
}
#endif

typedef struct {
    GRegex *regex_ok;
    GRegex *regex_connect;
    GRegex *regex_detailed_error;
    GRegex *regex_unknown_error;
    GRegex *regex_connect_failed;
} MMSerialParserV1;

gpointer
mm_serial_parser_v1_new (void)
{
    MMSerialParserV1 *parser;
    GRegexCompileFlags flags = G_REGEX_DOLLAR_ENDONLY | G_REGEX_RAW | G_REGEX_OPTIMIZE;

    parser = g_slice_new (MMSerialParserV1);

    parser->regex_ok = g_regex_new ("\\r\\nOK\\r\\n$", flags, 0, NULL);
    parser->regex_connect = g_regex_new ("\\r\\nCONNECT\\s*\\d*\\r\\n$", flags, 0, NULL);
    parser->regex_detailed_error = g_regex_new ("\\r\\n\\+CME ERROR: (\\d+)\\r\\n$", flags, 0, NULL);
    parser->regex_unknown_error = g_regex_new ("\\r\\n(ERROR)|(COMMAND NOT SUPPORT)\\r\\n$", flags, 0, NULL);
    parser->regex_connect_failed = g_regex_new ("\\r\\n(NO CARRIER)|(BUSY)|(NO ANSWER)|(NO DIALTONE)\\r\\n$", flags, 0, NULL);

    return parser;
}

gboolean
mm_serial_parser_v1_parse (gpointer data,
                           GString *response,
                           GError **error)
{
    MMSerialParserV1 *parser = (MMSerialParserV1 *) data;
    GMatchInfo *match_info;
    GError *local_error;
    int code;
    gboolean found = FALSE;

    /* First, check for successfule responses */

    found = g_regex_match_full (parser->regex_ok, response->str, response->len, 0, 0, NULL, NULL);
    if (found)
        remove_matches (parser->regex_ok, response);
    else
        found = g_regex_match_full (parser->regex_connect, response->str, response->len, 0, 0, NULL, NULL);

    if (found) {
        response_clean (response);
        return TRUE;
    }

    /* Now failures */
    code = MM_MOBILE_ERROR_UNKNOWN;
    local_error = NULL;

    found = g_regex_match_full (parser->regex_detailed_error,
                                response->str, response->len,
                                0, 0, &match_info, NULL);

    if (found) {
        char *str;

        str = g_match_info_fetch (match_info, 1);
        if (str) {
            code = atoi (str);
            g_free (str);
        }
        g_match_info_free (match_info);
    } else 
        found = g_regex_match_full (parser->regex_unknown_error, response->str, response->len, 0, 0, NULL, NULL);

    if (found)
        local_error = mm_mobile_error_for_code (code);
    else {
        found = g_regex_match_full (parser->regex_connect_failed,
                                    response->str, response->len,
                                    0, 0, &match_info, NULL);
        if (found) {
            char *str;

            str = g_match_info_fetch (match_info, 1);
            if (str) {
                if (!strcmp (str, "NO CARRIER"))
                    code = MM_MODEM_CONNECT_ERROR_NO_CARRIER;
                else if (!strcmp (str, "BUSY"))
                    code = MM_MODEM_CONNECT_ERROR_BUSY;
                else if (!strcmp (str, "NO ANSWER"))
                    code = MM_MODEM_CONNECT_ERROR_NO_ANSWER;
                else if (!strcmp (str, "NO DIALTONE"))
                    code = MM_MODEM_CONNECT_ERROR_NO_DIALTONE;
                else
                    /* uhm... make something up (yes, ok, lie!). */
                    code = MM_MODEM_CONNECT_ERROR_NO_CARRIER;

                g_free (str);
            }
            g_match_info_free (match_info);

            local_error = mm_modem_connect_error_for_code (code);
        }
    }

    if (local_error) {
        g_debug ("Got failure code %d: %s", local_error->code, local_error->message);
        g_propagate_error (error, local_error);
    }

    return found;
}

void
mm_serial_parser_v1_destroy (gpointer data)
{
    MMSerialParserV1 *parser = (MMSerialParserV1 *) data;

    g_regex_unref (parser->regex_ok);
    g_regex_unref (parser->regex_connect);
    g_regex_unref (parser->regex_detailed_error);
    g_regex_unref (parser->regex_unknown_error);
    g_regex_unref (parser->regex_connect_failed);

    g_slice_free (MMSerialParserV1, data);
}
