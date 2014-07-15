/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * location-watcher -- Watch location information from ModemManager
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2014 Aleksander Morgado <aleksander@aleksander.es>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#define _LIBMM_INSIDE_MMCLI
#include <libmm-glib.h>

/* Globals */
static GMainLoop       *loop;
static MMManager       *manager;
static GDBusConnection *connection;
static GHashTable      *location_db;

/* Context */
static gboolean no_setup_flag;
static gboolean with_getter_flag;
static gboolean with_property_flag;
static GOptionEntry entries[] = {
    { "no-setup", 'n', 0, G_OPTION_ARG_NONE, &no_setup_flag,
      "No location setup, just watch",
      NULL
    },
    { "with-getter", 'g', 0, G_OPTION_ARG_NONE, &with_getter_flag,
      "Use getter to query location (default)",
      NULL
    },
    { "with-property", 'p', 0, G_OPTION_ARG_NONE, &with_property_flag,
      "Read location from the property, don't use getter",
      NULL
    },
    { NULL }
};

typedef struct {
    MMModem         *modem;
    guint            state_changed_id;
    MMModemLocation *location;
    guint            location_updated_id;
} LocationInfo;

static void
location_info_free (LocationInfo *info)
{
    if (!info)
        return;

    if (g_signal_handler_is_connected (info->location, info->location_updated_id)) {
        g_signal_handler_disconnect (info->location, info->location_updated_id);
        if (!no_setup_flag)
            mm_modem_location_setup_sync (info->location,
                                          MM_MODEM_LOCATION_SOURCE_NONE,
                                          FALSE, /* signal location */
                                          NULL,  /* cancellable */
                                          NULL);
    }
    g_object_unref (info->location);

    if (g_signal_handler_is_connected (info->modem, info->state_changed_id))
        g_signal_handler_disconnect (info->modem, info->state_changed_id);
    g_object_unref (info->modem);

    g_slice_free (LocationInfo, info);
}

static void
signals_handler (int signum)
{
    if (loop && g_main_loop_is_running (loop)) {
        g_printerr ("%s\n",
                    "cancelling the main loop...\n");
        g_main_loop_quit (loop);
    }
}

static void
untrack_location (MMModemLocation *location)
{
    g_hash_table_remove (location_db, mm_modem_location_get_path (location));
}

static void
location_print_and_free (MMModemLocation       *location,
                         MMModemLocationSource  mask,
                         MMLocation3gpp        *location_3gpp,
                         MMLocationGpsNmea     *location_gps_nmea,
                         MMLocationGpsRaw      *location_gps_raw,
                         MMLocationCdmaBs      *location_cdma_bs)
{
    if (location_3gpp) {
        if (mask & MM_MODEM_LOCATION_SOURCE_3GPP_LAC_CI)
            g_print ("[%s,3GPP] MCC/MNC: '%u/%u', LAC: '%lu', CID: '%lu'\n",
                     mm_modem_location_get_path (location),
                     mm_location_3gpp_get_mobile_country_code (location_3gpp),
                     mm_location_3gpp_get_mobile_network_code (location_3gpp),
                     mm_location_3gpp_get_location_area_code (location_3gpp),
                     mm_location_3gpp_get_cell_id (location_3gpp));
        g_object_unref (location_3gpp);
    }

    if (location_gps_nmea) {
        if (mask & MM_MODEM_LOCATION_SOURCE_GPS_NMEA) {
            gchar *full;

            full = mm_location_gps_nmea_build_full (location_gps_nmea);
            g_print ("[%s,GPS NMEA]:\n"
                     "%s\n",
                     mm_modem_location_get_path (location),
                     full);
            g_free (full);
        }
        g_object_unref (location_gps_nmea);
    }

    if (location_gps_raw) {
        if (mask & MM_MODEM_LOCATION_SOURCE_GPS_RAW)
            g_print ("[%s GPS raw]: UTC: '%s', Lat: '%lf', Lon: '%lf', Alt: '%lf'\n",
                     mm_modem_location_get_path (location),
                     mm_location_gps_raw_get_utc_time (location_gps_raw),
                     mm_location_gps_raw_get_longitude (location_gps_raw),
                     mm_location_gps_raw_get_latitude (location_gps_raw),
                     mm_location_gps_raw_get_altitude (location_gps_raw));
        g_object_unref (location_gps_raw);
    }

    if (location_cdma_bs) {
        if (mask & MM_MODEM_LOCATION_SOURCE_CDMA_BS)
            g_print ("[%s CDMA BS]: Lat: '%lf', Lon: '%lf'\n",
                     mm_modem_location_get_path (location),
                     mm_location_cdma_bs_get_latitude (location_cdma_bs),
                     mm_location_cdma_bs_get_longitude (location_cdma_bs));
        g_object_unref (location_cdma_bs);
    }
}

static void
get_full_ready (MMModemLocation *location,
                GAsyncResult    *res,
                gpointer         user_data)
{
    guint              mask;
    GError            *error             = NULL;
    MMLocation3gpp    *location_3gpp     = NULL;
    MMLocationGpsNmea *location_gps_nmea = NULL;
    MMLocationGpsRaw  *location_gps_raw  = NULL;
    MMLocationCdmaBs  *location_cdma_bs  = NULL;

    mask = GPOINTER_TO_UINT (user_data);

    if (!mm_modem_location_get_full_finish (location,
                                            res,
                                            &location_3gpp,
                                            &location_gps_nmea,
                                            &location_gps_raw,
                                            &location_cdma_bs,
                                            &error)) {
        g_printerr ("[%s] error: couldn't retrieve full location info: %s",
                    mm_modem_location_get_path (location),
                    error->message);
        return;
    }

    location_print_and_free (location,
                             (MMModemLocationSource) mask,
                             location_3gpp,
                             location_gps_nmea,
                             location_gps_raw,
                             location_cdma_bs);
}

static void
location_updated_signal_cb (MMModemLocation       *location,
                            MMModemLocationSource  mask)
{
    gchar *str;

    str = mm_modem_location_source_build_string_from_mask (mask);
    g_print ("[%s] location update notification: '%s'\n",
             mm_modem_location_get_path (location),
             str);
    g_free (str);

    mm_modem_location_get_full (location,
                                NULL, /* cancellable */
                                (GAsyncReadyCallback) get_full_ready,
                                GUINT_TO_POINTER (mask));
}

static void
location_property_updated_cb (MMModemLocation *location,
                              GParamSpec      *spec)
{
    /* Read directly the property? */
    GVariantIter           iter;
    MMModemLocationSource  source;
    GVariant              *value;
    GVariant              *dictionary = NULL;
    MMLocation3gpp        *location_3gpp     = NULL;
    MMLocationGpsNmea     *location_gps_nmea = NULL;
    MMLocationGpsRaw      *location_gps_raw  = NULL;
    MMLocationCdmaBs      *location_cdma_bs  = NULL;
    MMModemLocationSource  mask;

    g_object_get (location, "location", &dictionary, NULL);
    if (!dictionary) {
        g_printerr ("[%s] error: couldn't retrieve full location info: unable to read property",
                    mm_modem_location_get_path (location));
        return;
    }

    g_variant_iter_init (&iter, dictionary);
    mask = MM_MODEM_LOCATION_SOURCE_NONE;
    while (g_variant_iter_next (&iter, "{uv}", &source, &value)) {
        GError *error = NULL;

        mask |= source;

        switch (source) {
        case MM_MODEM_LOCATION_SOURCE_3GPP_LAC_CI:
            location_3gpp = mm_location_3gpp_new_from_string_variant (value, &error);
            break;
        case MM_MODEM_LOCATION_SOURCE_GPS_NMEA:
            location_gps_nmea = mm_location_gps_nmea_new_from_string_variant (value, &error);
            break;
        case MM_MODEM_LOCATION_SOURCE_GPS_RAW:
            location_gps_raw = mm_location_gps_raw_new_from_dictionary (value, &error);
            break;
        case MM_MODEM_LOCATION_SOURCE_CDMA_BS:
            location_cdma_bs = mm_location_cdma_bs_new_from_dictionary (value, &error);
            break;
        default:
            g_warn_if_reached ();
            break;
        }

        if (error) {
            gchar *str;

            str = mm_modem_location_source_build_string_from_mask (source);
            g_printerr ("[%s] error reading '%s' location: %s",
                        mm_modem_location_get_path (location),
                        str,
                        error->message);
            g_error_free (error);
        }
        g_variant_unref (value);
    }

    location_print_and_free (location,
                             mask,
                             location_3gpp,
                             location_gps_nmea,
                             location_gps_raw,
                             location_cdma_bs);
}

static void
location_setup_ready (MMModemLocation *location,
                      GAsyncResult    *res)
{
    GError *error = NULL;

    if (!mm_modem_location_setup_finish (location, res, &error)) {
        g_printerr ("[%s] error: couldn't setup location: '%s'\n",
                    mm_modem_location_get_path (location),
                    error->message);
        g_error_free (error);
        g_main_loop_quit (loop);
        return;
    }

    g_print ("[%s] location setup updated successfully\n",
             mm_modem_location_get_path (location));
}

static void
state_changed_cb (MMModem                  *modem,
                  MMModemState              old_state,
                  MMModemState              new_state,
                  MMModemStateChangeReason  reason)
{
    LocationInfo *info;

    info = g_hash_table_lookup (location_db, mm_modem_get_path (modem));
    if (!info)
        return;

    /* If going into enabled state, enable all location info */
    if (old_state < MM_MODEM_STATE_ENABLED && new_state >= MM_MODEM_STATE_ENABLED) {
        gboolean signal_location = FALSE;

        g_print ("[%s] enabling location...\n",
                 mm_modem_location_get_path (info->location));

        /* Track updates */
        if (with_property_flag) {
            signal_location = TRUE;
            info->location_updated_id = g_signal_connect (info->location,
                                                          "notify::location",
                                                          G_CALLBACK (location_property_updated_cb),
                                                          NULL);
        } else if (with_getter_flag)
            info->location_updated_id = g_signal_connect (info->location,
                                                          "location-updated",
                                                          G_CALLBACK (location_updated_signal_cb),
                                                          NULL);
        else
            g_assert_not_reached ();

        /* Request to enable all location capabilities */
        if (!no_setup_flag)
            mm_modem_location_setup (info->location,
                                     mm_modem_location_get_capabilities (info->location),
                                     signal_location,
                                     NULL, /* cancellable */
                                     (GAsyncReadyCallback) location_setup_ready,
                                     NULL);
        return;
    }

    /* If going out of enabled state, disable all location info */
    if (old_state >= MM_MODEM_STATE_ENABLED && new_state < MM_MODEM_STATE_ENABLED) {
        g_print ("[%s] disabling location...\n",
                 mm_modem_location_get_path (info->location));
        /* Request to disable all location capabilities */
        if (!no_setup_flag)
            mm_modem_location_setup (info->location,
                                     MM_MODEM_LOCATION_SOURCE_NONE,
                                     FALSE, /* signal location */
                                     NULL,  /* cancellable */
                                     (GAsyncReadyCallback) location_setup_ready,
                                     NULL);
        /* Untrack updates */
        if (g_signal_handler_is_connected (info->location, info->location_updated_id))
            g_signal_handler_disconnect (info->location, info->location_updated_id);
        info->location_updated_id = 0;
        return;
    }
}

static void
track_location (MMModem         *modem,
                MMModemLocation *location)
{
    MMModemLocationSource capabilities;
    gchar *str;
    LocationInfo *info;

    capabilities = mm_modem_location_get_capabilities (location);
    str = mm_modem_location_source_build_string_from_mask (capabilities);
    g_print ("[%s] found modem with location capabilities: '%s'\n",
             mm_modem_location_get_path (location),
             str);
    g_free (str);

    /* Keep location info */
    info = g_slice_new0 (LocationInfo);
    info->modem = g_object_ref (modem);
    info->state_changed_id = g_signal_connect (modem,
                                               "state-changed",
                                               G_CALLBACK (state_changed_cb),
                                               NULL);
    info->location = g_object_ref (location);
    g_hash_table_replace (location_db,
                          g_strdup (mm_modem_location_get_path (location)),
                          info);

    /* If modem is already enabled, trigger location enable */
    if (mm_modem_get_state (modem) >= MM_MODEM_STATE_ENABLED)
        state_changed_cb (modem,
                          MM_MODEM_STATE_UNKNOWN,
                          mm_modem_get_state (modem),
                          MM_MODEM_STATE_CHANGE_REASON_UNKNOWN);
}

static void
interface_added_cb (MMManager      *man,
                    MMObject       *obj,
                    GDBusInterface *interface,
                    gpointer        user_data)
{
    if (!MM_IS_MODEM_LOCATION (interface)) {
        g_print ("unneeded interface added...\n");
        return;
    }

    g_print ("location interface added...\n");
    track_location (mm_object_peek_modem (obj), MM_MODEM_LOCATION (interface));
}

static void
interface_removed_cb (MMManager      *man,
                      MMObject       *obj,
                      GDBusInterface *interface,
                      gpointer        user_data)
{
    if (!MM_IS_MODEM_LOCATION (interface))
        return;
    untrack_location (MM_MODEM_LOCATION (interface));
}

static void
manager_new_ready (GObject      *source,
                   GAsyncResult *res)
{
    GError *error = NULL;
    GList *initial_objects, *l;

    manager = mm_manager_new_finish (res, &error);
    if (!manager) {
        g_printerr ("error: couldn't create manager: %s\n",
                    error ? error->message : "unknown error");
        exit (EXIT_FAILURE);
    }

    g_print ("manager acquired...\n");

    /* Process initial objects */
    initial_objects = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (manager));
    for (l = initial_objects; l; l = g_list_next (l)) {
        MMObject *obj;
        MMModemLocation *location;

        obj = MM_OBJECT (l->data);
        location = mm_object_peek_modem_location (obj);
        if (location)
            track_location (mm_object_peek_modem (obj), location);
    }
    g_list_free_full (initial_objects, g_object_unref);

    /* Connect for interface updates */
    g_signal_connect (manager,
                      "interface-added",
                      G_CALLBACK (interface_added_cb),
                      NULL);
    g_signal_connect (manager,
                      "interface-removed",
                      G_CALLBACK (interface_removed_cb),
                      NULL);
}

static void
bus_get_ready (GObject      *source,
               GAsyncResult *res)
{
    GError *error = NULL;

    connection = g_bus_get_finish (res, &error);
    if (!connection) {
        g_printerr ("error: couldn't get bus: %s\n",
                    error ? error->message : "unknown error");
        exit (EXIT_FAILURE);
    }

    g_print ("system bus acquired...\n");

    mm_manager_new (connection,
                    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
                    NULL, /* cancellable */
                    (GAsyncReadyCallback) manager_new_ready,
                    NULL);
}

gint
main (gint argc, gchar **argv)
{
    GOptionContext *context;

    setlocale (LC_ALL, "");

    g_type_init ();

    /* Setup option context, process it and destroy it */
    context = g_option_context_new ("- Watch location through ModemManager");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free (context);

    if (with_getter_flag && with_property_flag) {
        g_printerr ("error: cannot use --with-getter and --with-property at the same time\n");
        return EXIT_FAILURE;
    }

    /* If none explicitly given, assume getter */
    if (!with_getter_flag && !with_property_flag)
        with_getter_flag = TRUE;

    /* Setup signals */
    signal (SIGINT, signals_handler);
    signal (SIGHUP, signals_handler);
    signal (SIGTERM, signals_handler);

    /* Setup tracking table (key: path, value: location interface) */
    location_db = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         (GDestroyNotify) location_info_free);

    /* Setup dbus connection to use */
    g_bus_get (G_BUS_TYPE_SYSTEM,
               NULL, /* cancellable */
               (GAsyncReadyCallback) bus_get_ready,
               NULL);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    /* Clean exit */
    g_main_loop_unref (loop);
    g_object_unref (connection);
    g_hash_table_unref (location_db);
    return EXIT_SUCCESS;
}
