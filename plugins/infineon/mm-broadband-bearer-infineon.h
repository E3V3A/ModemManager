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

#ifndef MM_BROADBAND_BEARER_INFINEON_H
#define MM_BROADBAND_BEARER_INFINEON_H

#include <glib.h>
#include <glib-object.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-broadband-bearer.h"
#include "mm-broadband-modem-infineon.h"

#define MM_TYPE_BROADBAND_BEARER_INFINEON            (mm_broadband_bearer_infineon_get_type ())
#define MM_BROADBAND_BEARER_INFINEON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_BROADBAND_BEARER_INFINEON, MMBroadbandBearerInfineon))
#define MM_BROADBAND_BEARER_INFINEON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_BROADBAND_BEARER_INFINEON, MMBroadbandBearerInfineonClass))
#define MM_IS_BROADBAND_BEARER_INFINEON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_BROADBAND_BEARER_INFINEON))
#define MM_IS_BROADBAND_BEARER_INFINEON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_BROADBAND_BEARER_INFINEON))
#define MM_BROADBAND_BEARER_INFINEON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_BROADBAND_BEARER_INFINEON, MMBroadbandBearerInfineonClass))

typedef struct _MMBroadbandBearerInfineon MMBroadbandBearerInfineon;
typedef struct _MMBroadbandBearerInfineonClass MMBroadbandBearerInfineonClass;
typedef struct _MMBroadbandBearerInfineonPrivate MMBroadbandBearerInfineonPrivate;

struct _MMBroadbandBearerInfineon {
    MMBroadbandBearer parent;
    MMBroadbandBearerInfineonPrivate *priv;
};

struct _MMBroadbandBearerInfineonClass {
    MMBroadbandBearerClass parent;
};

GType mm_broadband_bearer_infineon_get_type (void);

/* Default 3GPP bearer creation implementation */
void mm_broadband_bearer_infineon_new (MMBroadbandModemInfineon *modem,
                                       MMBearerProperties *config,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);
MMBearer *mm_broadband_bearer_infineon_new_finish (GAsyncResult *res,
                                                   GError **error);

#endif /* MM_BROADBAND_BEARER_INFINEON_H */
