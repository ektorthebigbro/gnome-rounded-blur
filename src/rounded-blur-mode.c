/* rounded-blur-mode.c
 *
 * Copyright 2025 GNOME Rounded Blur
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "rounded-blur-effect.h"

GType
gb_blur_mode_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (etype == 0))
    {
      static const GEnumValue values[] = {
        { GB_BLUR_MODE_ACTOR, "GB_BLUR_MODE_ACTOR", "actor" },
        { GB_BLUR_MODE_BACKGROUND, "GB_BLUR_MODE_BACKGROUND", "background" },
        { 0, NULL, NULL }
      };
      etype = g_enum_register_static (g_intern_static_string ("GbBlurMode"), values);
    }
  return etype;
}
