/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Haze Project
 *
 * Haze status emitter implementation for gnome-rounded-blur.
 * All functions return static storage – callers must not free the pointers.
 */

#include "haze-status.h"

#define HAZE_STATUS_CONTRACT_VERSION_VALUE 1
#define HAZE_MODULE_ID_VALUE               "gnome-rounded-blur"
#define HAZE_MODULE_VERSION_VALUE          "1.1.0"
#define HAZE_MODULE_STATE_VALUE            "active"

gint
haze_status_get_contract_version (void)
{
  return HAZE_STATUS_CONTRACT_VERSION_VALUE;
}

const gchar *
haze_status_get_module_id (void)
{
  return HAZE_MODULE_ID_VALUE;
}

const gchar *
haze_status_get_version (void)
{
  return HAZE_MODULE_VERSION_VALUE;
}

const gchar *
haze_status_get_state (void)
{
  return HAZE_MODULE_STATE_VALUE;
}
