/* rounded-blur-effect.h
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

#pragma once

#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef enum
{
  GB_BLUR_MODE_ACTOR,
  GB_BLUR_MODE_BACKGROUND,
} GbBlurMode;

#define GB_TYPE_BLUR_EFFECT (gb_blur_effect_get_type())
#define GB_TYPE_BLUR_MODE (gb_blur_mode_get_type())

G_DECLARE_FINAL_TYPE (GbBlurEffect, gb_blur_effect, GB, BLUR_EFFECT, ClutterEffect)

GType gb_blur_mode_get_type (void) G_GNUC_CONST;

GbBlurEffect *gb_blur_effect_new (void);

int gb_blur_effect_get_radius (GbBlurEffect *self);
void gb_blur_effect_set_radius (GbBlurEffect *self,
                                   int            radius);

float gb_blur_effect_get_brightness (GbBlurEffect *self);
void gb_blur_effect_set_brightness (GbBlurEffect *self,
                                       float          brightness);

GbBlurMode gb_blur_effect_get_mode (GbBlurEffect *self);
void gb_blur_effect_set_mode (GbBlurEffect *self,
                                 GbBlurMode    mode);

float gb_blur_effect_get_corner_radius (GbBlurEffect *self);
void gb_blur_effect_set_corner_radius (GbBlurEffect *self,
                                        float          corner_radius);

G_END_DECLS
