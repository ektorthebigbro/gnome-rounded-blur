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
#include "adaptive-brightness.h"

G_BEGIN_DECLS

#define GB_TYPE_LIQUID_GLASS_EFFECT \
  (gb_liquid_glass_effect_get_type ())

G_DECLARE_FINAL_TYPE (GbLiquidGlassEffect,
                      gb_liquid_glass_effect,
                      GB,
                      LIQUID_GLASS_EFFECT,
                      ClutterEffect)

GbLiquidGlassEffect *gb_liquid_glass_effect_new (void);

int gb_liquid_glass_effect_get_radius (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_radius (GbLiquidGlassEffect *self,
                                        int                  radius);

float gb_liquid_glass_effect_get_brightness (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_brightness (GbLiquidGlassEffect *self,
                                            float                brightness);

gboolean gb_liquid_glass_effect_get_adaptive_brightness (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_adaptive_brightness (GbLiquidGlassEffect *self,
                                                     gboolean             adaptive_brightness);

float gb_liquid_glass_effect_get_adaptive_brightness_strength (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_adaptive_brightness_strength (GbLiquidGlassEffect *self,
                                                              float                strength);

float gb_liquid_glass_effect_get_adaptive_brightness_minimum (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_adaptive_brightness_minimum (GbLiquidGlassEffect *self,
                                                             float                minimum);

GbAdaptiveBrightnessQuality gb_liquid_glass_effect_get_adaptive_brightness_quality (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_adaptive_brightness_quality (GbLiquidGlassEffect         *self,
                                                             GbAdaptiveBrightnessQuality  quality);

GbBlurMode gb_liquid_glass_effect_get_mode (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_mode (GbLiquidGlassEffect *self,
                                      GbBlurMode           mode);

float gb_liquid_glass_effect_get_corner_radius (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_corner_radius (GbLiquidGlassEffect *self,
                                               float                corner_radius);

float gb_liquid_glass_effect_get_highlight (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_highlight (GbLiquidGlassEffect *self,
                                           float                highlight);

float gb_liquid_glass_effect_get_glow_weight (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_glow_weight (GbLiquidGlassEffect *self,
                                             float                glow_weight);

float gb_liquid_glass_effect_get_glow_bias (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_glow_bias (GbLiquidGlassEffect *self,
                                           float                glow_bias);

float gb_liquid_glass_effect_get_glow_bevel (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_glow_bevel (GbLiquidGlassEffect *self,
                                            float                glow_bevel);

float gb_liquid_glass_effect_get_glow_smooth (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_glow_smooth (GbLiquidGlassEffect *self,
                                             float                glow_smooth);

float gb_liquid_glass_effect_get_refraction (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_refraction (GbLiquidGlassEffect *self,
                                            float                refraction);

float gb_liquid_glass_effect_get_depth (GbLiquidGlassEffect *self);
void gb_liquid_glass_effect_set_depth (GbLiquidGlassEffect *self,
                                       float                depth);

G_END_DECLS
