/* blur-debug.h
 *
 * Copyright 2026 GNOME Rounded Blur
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

extern gboolean gb_blur_debug_logging_enabled;

#ifndef __GI_SCANNER__
void gb_set_debug_logging_enabled (gboolean enabled);
gboolean gb_get_debug_logging_enabled (void);
void gb_blur_debug_log (const char *format, ...) G_GNUC_PRINTF (1, 2);
#endif

G_END_DECLS

#define GB_BLUR_DEBUG(...) \
  G_STMT_START { \
    if (G_UNLIKELY (gb_blur_debug_logging_enabled)) \
      gb_blur_debug_log (__VA_ARGS__); \
  } G_STMT_END
