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
 * Copyright (C) 2009 - 2011 Red Hat, Inc.
 */

#include <config.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <glib-unix.h>

#include "ModemManager.h"

#include "mm-manager.h"
#include "mm-log.h"
#include "mm-context.h"

#if !defined(MM_DIST_VERSION)
# define MM_DIST_VERSION VERSION
#endif

/* Maximum time to wait for all modems to get disabled and removed */
#define MAX_SHUTDOWN_TIME_SECS 20

/* Maximum time to wait after last modem disappears */
#define MAX_WAIT_AFTER_LAST_MODEM_SECS 10
guint after_last_modem_timeout_id;

static GMainLoop *loop;
static MMManager *manager;

static gboolean
quit_cb (gpointer user_data)
{
	mm_info ("Caught signal, shutting down...");
    if (loop)
        g_idle_add ((GSourceFunc) g_main_loop_quit, loop);
    else
        _exit (0);
    return FALSE;
}

static gboolean
after_last_modem_stop_loop_cb (gpointer user_data)
{
    after_last_modem_timeout_id = 0;
	mm_info ("No modems to manage, shutting down...");
    if (loop)
        g_idle_add ((GSourceFunc) g_main_loop_quit, loop);
    return FALSE;
}

static void
manager_num_modems_changed (MMManager *_manager)
{
    if (mm_manager_get_num_modems (manager) == 0) {
        g_assert (after_last_modem_timeout_id == 0);
        after_last_modem_timeout_id = g_timeout_add_seconds (MAX_WAIT_AFTER_LAST_MODEM_SECS,
                                                             (GSourceFunc)after_last_modem_stop_loop_cb,
                                                             NULL);
        return;
    }

    /* At least one modem, remove timeout if any */
    if (after_last_modem_timeout_id) {
        g_source_remove (after_last_modem_timeout_id);
        after_last_modem_timeout_id = 0;
    }
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
    GError *error = NULL;

    mm_dbg ("Bus acquired, creating manager...");

    /* Create Manager object */
    g_assert (!manager);
    manager = mm_manager_new (connection, &error);
    if (!manager) {
        mm_warn ("Could not create manager: %s", error->message);
        g_error_free (error);
        g_main_loop_quit (loop);
        return;
    }

    /* If exit requested when no modems available, set it up */
    if (mm_context_get_exit_without_modems ()) {
        manager_num_clients_changed (manager);
        g_signal_connect (manager,
                          "notify::" MM_MANAGER_NUM_CLIENTS,
                          G_CALLBACK (manager_num_clients_changed),
                          NULL);
    }
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{
    mm_dbg ("Service name '%s' was acquired", name);

    /* Launch automatic scan for devices */
    g_assert (manager);
    mm_manager_start (manager, FALSE);
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
    /* Note that we're not allowing replacement, so once the name acquired, the
     * process won't lose it. */
    if (!name)
        mm_warn ("Could not get the system bus. Make sure "
                 "the message bus daemon is running!");
    else
        mm_warn ("Could not acquire the '%s' service name", name);

    g_main_loop_quit (loop);
}

int
main (int argc, char *argv[])
{
    GMainLoop *inner;
    GError *err = NULL;
    guint name_id;

    g_type_init ();

    /* Setup application context */
    mm_context_init (argc, argv);

    if (!mm_log_setup (mm_context_get_log_level (),
                       mm_context_get_log_file (),
                       mm_context_get_timestamps (),
                       mm_context_get_relative_timestamps (),
                       mm_context_get_debug (),
                       &err)) {
        g_warning ("Failed to set up logging: %s", err->message);
        g_error_free (err);
        exit (1);
    }

    g_unix_signal_add (SIGTERM, quit_cb, NULL);
    g_unix_signal_add (SIGINT, quit_cb, NULL);

    mm_info ("ModemManager (version " MM_DIST_VERSION ") starting...");

    /* Acquire name, don't allow replacement */
    name_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
                              MM_DBUS_SERVICE,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              bus_acquired_cb,
                              name_acquired_cb,
                              name_lost_cb,
                              NULL,
                              NULL);

    /* Go into the main loop */
    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    /* Clear the global variable, so that subsequent requests to
     * exit succeed. */
    inner = loop;
    loop = NULL;

    if (manager) {
        GTimer *timer;

        mm_manager_shutdown (manager);

        /* Wait for all modems to be disabled and removed, but don't wait
         * forever: if disabling the modems takes longer than 20s, just
         * shutdown anyway. */
        timer = g_timer_new ();
        while (mm_manager_get_num_modems (manager) &&
               g_timer_elapsed (timer, NULL) < (gdouble)MAX_SHUTDOWN_TIME_SECS) {
            GMainContext *ctx = g_main_loop_get_context (inner);

            g_main_context_iteration (ctx, FALSE);
            g_usleep (50);
        }

        if (mm_manager_num_modems (manager))
            mm_warn ("Disabling modems took too long, "
                     "shutting down with '%u' modems around",
                     mm_manager_num_modems (manager));

        g_object_unref (manager);
        g_timer_destroy (timer);
    }

    g_main_loop_unref (inner);

    g_bus_unown_name (name_id);

    mm_info ("ModemManager is shut down");

    mm_log_shutdown ();

    return 0;
}
