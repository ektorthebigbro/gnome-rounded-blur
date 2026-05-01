/* liquid-glass-effect.h
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

#include "rounded-blur-effect.h"

G_BEGIN_DECLS

#define GB_TYPE_LIQUID_GLASS_EFFECT (gb_liquid_glass_effect_get_type())

G_DECLARE_FINAL_TYPE (GbLiquidGlassEffect, gb_liquid_glass_effect, GB, LIQUID_GLASS_EFFECT, ClutterEffect)

GbLiquidGlassEffect *gb_liquid_glass_effect_new (void);

int gb_liquid_glass_effect_get_radius (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_radius (GbLiquidGlassEffect *self,
                                   int            radius);

float gb_liquid_glass_effect_get_brightness (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_brightness (GbLiquidGlassEffect *self,
                                       float          brightness);

GbBlurMode gb_liquid_glass_effect_get_mode (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_mode (GbLiquidGlassEffect *self,
                                 GbBlurMode    mode);

float gb_liquid_glass_effect_get_corner_radius (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_corner_radius (GbLiquidGlassEffect *self,
                                        float          corner_radius);

float gb_liquid_glass_effect_get_saturation (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_saturation (GbLiquidGlassEffect *self,
                                            float                saturation);

float gb_liquid_glass_effect_get_tint (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_tint (GbLiquidGlassEffect *self,
                                      float                tint);

float gb_liquid_glass_effect_get_highlight (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_highlight (GbLiquidGlassEffect *self,
                                           float                highlight);

float gb_liquid_glass_effect_get_refraction (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_refraction (GbLiquidGlassEffect *self,
                                            float                refraction);

G_END_DECLS
