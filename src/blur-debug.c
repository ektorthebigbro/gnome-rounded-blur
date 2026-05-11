/* blur-debug.c
 *
 * Copyright 2026 GNOME Rounded Blur
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "blur-debug.h"

#include <stdarg.h>

gboolean gb_blur_debug_logging_enabled = FALSE;

void
gb_set_debug_logging_enabled (gboolean enabled)
{
  if (gb_blur_debug_logging_enabled == !!enabled)
    return;

  gb_blur_debug_logging_enabled = !!enabled;
  g_message ("[gnome-rounded-blur] debug logging %s",
             gb_blur_debug_logging_enabled ? "enabled" : "disabled");
}

gboolean
gb_get_debug_logging_enabled (void)
{
  return gb_blur_debug_logging_enabled;
}

void
gb_blur_debug_log (const char *format,
                   ...)
{
  va_list args;
  g_autofree char *message = NULL;

  if (!gb_blur_debug_logging_enabled)
    return;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  g_message ("[gnome-rounded-blur] %s", message);
}
