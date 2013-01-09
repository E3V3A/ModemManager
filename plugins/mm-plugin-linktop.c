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
 * Copyright (C) 2009 Red Hat, Inc.
 */

#include <string.h>
#include <gmodule.h>
#include "mm-plugin-linktop.h"
#include "mm-modem-linktop.h"
#include "mm-log.h"

G_DEFINE_TYPE (MMPluginLinktop, mm_plugin_linktop, MM_TYPE_PLUGIN_BASE)

int mm_plugin_major_version = MM_PLUGIN_MAJOR_VERSION;
int mm_plugin_minor_version = MM_PLUGIN_MINOR_VERSION;

G_MODULE_EXPORT MMPlugin *
mm_plugin_create (void)
{
    return MM_PLUGIN (g_object_new (MM_TYPE_PLUGIN_LINKTOP,
                                    MM_PLUGIN_BASE_NAME, "Linktop",
                                    NULL));
}

/*****************************************************************************/

static guint32
get_level_for_capabilities (guint32 capabilities)
{
    if (capabilities & MM_PLUGIN_BASE_PORT_CAP_GSM)
        return 10;
    return 0;
}

static void
probe_result (MMPluginBase *base,
              MMPluginBaseSupportsTask *task,
              guint32 capabilities,
              gpointer user_data)
{
    mm_plugin_base_supports_task_complete (task, get_level_for_capabilities (capabilities));
}

static MMPluginSupportsResult
supports_port (MMPluginBase *base,
               MMModem *existing,
               MMPluginBaseSupportsTask *task)
{
    GUdevDevice *port;
    const char *subsys, *name;
    guint16 vendor = 0;

    /* Can't do anything with non-serial ports */
    port = mm_plugin_base_supports_task_get_port (task);
    if (strcmp (g_udev_device_get_subsystem (port), "tty"))
        return MM_PLUGIN_SUPPORTS_PORT_UNSUPPORTED;

    subsys = g_udev_device_get_subsystem (port);
    name = g_udev_device_get_name (port);

    if (!mm_plugin_base_get_device_ids (base, subsys, name, &vendor, NULL))
        return MM_PLUGIN_SUPPORTS_PORT_UNSUPPORTED;

    if (vendor != 0x230d)
        return MM_PLUGIN_SUPPORTS_PORT_UNSUPPORTED;

    /* Check if a previous probing was already launched in this port */
    if (mm_plugin_base_supports_task_propagate_cached (task)) {
        guint32 level;

        /* A previous probing was already done, use its results */
        level = get_level_for_capabilities (mm_plugin_base_supports_task_get_probed_capabilities (task));
        if (level) {
            mm_plugin_base_supports_task_complete (task, level);
            return MM_PLUGIN_SUPPORTS_PORT_IN_PROGRESS;
        }
        return MM_PLUGIN_SUPPORTS_PORT_UNSUPPORTED;
    }

    /* Otherwise kick off a probe */
    if (mm_plugin_base_probe_port (base, task, 100000, NULL))
        return MM_PLUGIN_SUPPORTS_PORT_IN_PROGRESS;

    return MM_PLUGIN_SUPPORTS_PORT_UNSUPPORTED;
}

static MMModem *
grab_port (MMPluginBase *base,
           MMModem *existing,
           MMPluginBaseSupportsTask *task,
           GError **error)
{
    GUdevDevice *port = NULL;
    MMModem *modem = NULL;
    const char *name, *subsys, *devfile, *sysfs_path;
    guint32 caps;
    guint16 vendor = 0, product = 0;
    MMPortType ptype;
    MMAtPortFlags pflags = MM_AT_PORT_FLAG_NONE;

    port = mm_plugin_base_supports_task_get_port (task);
    g_assert (port);

    devfile = g_udev_device_get_device_file (port);
    if (!devfile) {
        g_set_error (error, 0, 0, "Could not get port's sysfs file.");
        return NULL;
    }

    subsys = g_udev_device_get_subsystem (port);
    name = g_udev_device_get_name (port);

    if (!mm_plugin_base_get_device_ids (base, subsys, name, &vendor, &product)) {
        g_set_error (error, 0, 0, "Could not get modem product ID.");
        return NULL;
    }

    caps = mm_plugin_base_supports_task_get_probed_capabilities (task);
    ptype = mm_plugin_base_probed_capabilities_to_port_type (caps);

    /* 3-endpoint AT-capable ports are more likely to be the primary port */
    if (ptype == MM_PORT_TYPE_AT) {
        if (mm_plugin_base_supports_task_get_num_interface_endpoints (task) == 3) {
            pflags = MM_AT_PORT_FLAG_PRIMARY;
            mm_dbg ("(%s/%s) hinting PRIMARY due to possible Interrupt endpoint", subsys, name);
        }
    }

    sysfs_path = mm_plugin_base_supports_task_get_physdev_path (task);
    if (!existing) {
        if (caps & MM_PLUGIN_BASE_PORT_CAP_GSM) {
            modem = mm_modem_linktop_new (sysfs_path,
                                        mm_plugin_base_supports_task_get_driver (task),
                                        mm_plugin_get_name (MM_PLUGIN (base)),
                                        vendor,
                                        product);
        }

        if (modem) {
            if (!mm_modem_grab_port (modem, subsys, name, ptype, pflags, NULL, error)) {
                g_object_unref (modem);
                return NULL;
            }
        }
    } else if (get_level_for_capabilities (caps)) {
        modem = existing;
        if (!mm_modem_grab_port (modem, subsys, name, ptype, pflags, NULL, error))
            return NULL;
    }

    return modem;
}

/*****************************************************************************/

static void
mm_plugin_linktop_init (MMPluginLinktop *self)
{
    g_signal_connect (self, "probe-result", G_CALLBACK (probe_result), NULL);
}

static void
mm_plugin_linktop_class_init (MMPluginLinktopClass *klass)
{
    MMPluginBaseClass *pb_class = MM_PLUGIN_BASE_CLASS (klass);

    pb_class->supports_port = supports_port;
    pb_class->grab_port = grab_port;
}
