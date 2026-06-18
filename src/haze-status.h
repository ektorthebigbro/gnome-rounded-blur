/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Haze Project
 *
 * Haze status emitter API for gnome-rounded-blur.
 * Provides a stable C ABI surface so the Haze extension can query module
 * identity, contract version, installed version, and runtime state without
 * probing pkg-config or spawning subprocesses at DBus call time.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/**
 * haze_status_get_contract_version:
 *
 * Returns the integer version of the Haze status emitter contract implemented
 * by this module build. The Haze extension uses this to detect whether the
 * emitter ABI is compatible before calling any other haze_status_* function.
 *
 * Returns: contract version integer (currently 1).
 */
gint haze_status_get_contract_version (void);

/**
 * haze_status_get_module_id:
 *
 * Returns the stable Haze module identifier string for this module.
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none): module identifier, e.g. "gnome-rounded-blur".
 */
const gchar *haze_status_get_module_id (void);

/**
 * haze_status_get_version:
 *
 * Returns the installed version string for this module.
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none): version string, e.g. "1.1.0".
 */
const gchar *haze_status_get_version (void);

/**
 * haze_status_get_state:
 *
 * Returns the runtime state token for this module.  The token is one of the
 * values the Haze compatibility engine recognises:
 *   "active"   – module is loaded and operating normally.
 *   "degraded" – module is loaded but some capability is unavailable.
 *   "inactive" – module is installed but not currently active.
 *
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none): state token string.
 */
const gchar *haze_status_get_state (void);

G_END_DECLS
