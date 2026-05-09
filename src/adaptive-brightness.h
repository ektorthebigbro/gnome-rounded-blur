/* adaptive-brightness.h
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

#include <glib.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

/**
 * GbAdaptiveBrightnessQuality:
 * @GB_ADAPTIVE_BRIGHTNESS_QUALITY_PERFORMANCE: Linear reduction curve, single
 *   luma tap, no pow() — cheapest GPU path. Matches the "performance" preset
 *   from the JS runtime constants (sampleCount=1, repaintResampleSkipCount=3).
 * @GB_ADAPTIVE_BRIGHTNESS_QUALITY_BALANCED: Eased pow() reduction curve,
 *   single luma tap — default quality. Matches "balanced" (sampleCount=2).
 * @GB_ADAPTIVE_BRIGHTNESS_QUALITY_QUALITY: Eased pow() reduction curve with a
 *   5-tap cross neighborhood luma average — most accurate. Matches "quality"
 *   (sampleCount=3, repaintResampleSkipCount=0).
 *
 * Each level compiles into a separate CoglPipeline so the GPU executes the
 * tightest possible shader for the chosen level with no runtime branching.
 */
typedef enum {
  GB_ADAPTIVE_BRIGHTNESS_QUALITY_PERFORMANCE = 0,
  GB_ADAPTIVE_BRIGHTNESS_QUALITY_BALANCED    = 1,
  GB_ADAPTIVE_BRIGHTNESS_QUALITY_QUALITY     = 2,
} GbAdaptiveBrightnessQuality;

/**
 * gb_adaptive_brightness_create_pipeline: (skip)
 * @base: a base CoglPipeline to copy (must already have layer 0 configured)
 * @quality: which performance level to compile into the pipeline
 *
 * Returns a new CoglPipeline copy with the adaptive-brightness GLSL snippet
 * for @quality baked in.  The caller takes ownership and must g_object_unref()
 * when done.
 */
CoglPipeline *gb_adaptive_brightness_create_pipeline (CoglPipeline               *base,
                                                       GbAdaptiveBrightnessQuality quality);

/**
 * gb_adaptive_brightness_apply: (skip)
 * @pipeline: brightness pipeline (created by gb_adaptive_brightness_create_pipeline)
 * @brightness_uniform: location of "brightness" (-1 to skip)
 * @adaptive_uniform: location of "u_adaptive_brightness" (-1 to skip)
 * @strength_uniform: location of "u_adaptive_brightness_strength" (-1 to skip)
 * @minimum_uniform: location of "u_adaptive_brightness_minimum" (-1 to skip)
 * @tex_size_uniform: location of "u_ab_tex_size" (-1 to skip; only used by QUALITY)
 * @paint_opacity: current paint opacity (0-255)
 * @brightness: base brightness value (0.0-1.0)
 * @adaptive: whether adaptive brightness is enabled
 * @strength: adaptive brightness strength (0.0-1.0)
 * @minimum: adaptive brightness minimum floor (0.0-1.0)
 * @tex_width: texture width in pixels (used by QUALITY for neighbour offsets)
 * @tex_height: texture height in pixels (used by QUALITY for neighbour offsets)
 *
 * Sets the paint color and all adaptive-brightness uniforms on @pipeline.
 */
void gb_adaptive_brightness_apply (CoglPipeline *pipeline,
                                   int           brightness_uniform,
                                   int           adaptive_uniform,
                                   int           strength_uniform,
                                   int           minimum_uniform,
                                   int           tex_size_uniform,
                                   uint8_t       paint_opacity,
                                   float         brightness,
                                   gboolean      adaptive,
                                   float         strength,
                                   float         minimum,
                                   float         tex_width,
                                   float         tex_height);

G_END_DECLS
