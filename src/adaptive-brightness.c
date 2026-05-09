/* adaptive-brightness.c
 *
 * Copyright 2025 GNOME Rounded Blur
 *
 * Adaptive brightness owns the per-quality GLSL shader variants and the
 * CoglPipeline factory that bakes each variant into a separate pipeline.
 * Having three distinct compiled pipelines means the GPU executes the tightest
 * possible shader for the active quality level with no runtime branching.
 *
 * Quality levels (GbAdaptiveBrightnessQuality):
 *
 *   PERFORMANCE  — single luma tap, linear (no pow) reduction curve.
 *                  Fastest math path; matches the JS "performance" preset.
 *
 *   BALANCED     — single luma tap, eased pow() reduction curve.
 *                  Default; matches the JS "balanced" preset.
 *
 *   QUALITY      — 5-tap cross neighbourhood luma average, eased pow() curve.
 *                  Most accurate; matches the JS "quality" preset.
 *                  Requires u_ab_tex_size to compute neighbour UV offsets.
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

#include "adaptive-brightness.h"

/* ---------------------------------------------------------------------------
 * Shared GLSL declarations (all three quality levels use the same uniforms
 * except QUALITY which also needs u_ab_tex_size).
 * --------------------------------------------------------------------------- */

static const gchar *ab_declarations_base =
  "uniform float brightness;\n"
  "uniform float u_adaptive_brightness;\n"
  "uniform float u_adaptive_brightness_strength;\n"
  "uniform float u_adaptive_brightness_minimum;\n";

static const gchar *ab_declarations_quality =
  "uniform float brightness;\n"
  "uniform float u_adaptive_brightness;\n"
  "uniform float u_adaptive_brightness_strength;\n"
  "uniform float u_adaptive_brightness_minimum;\n"
  "uniform vec2  u_ab_tex_size;\n";

/* ---------------------------------------------------------------------------
 * PERFORMANCE variant
 *
 * Single luma tap, linear (no pow) reduction.  The absence of pow() saves
 * one transcendental instruction per fragment.
 * --------------------------------------------------------------------------- */

static const gchar *ab_glsl_performance =
  "  float adaptive_strength = clamp(u_adaptive_brightness_strength, 0.0, 1.0);\n"
  "  float adaptive_brightness = brightness;\n"
  "  if (u_adaptive_brightness > 0.5 && adaptive_strength > 0.0)\n"
  "    {\n"
  "      const float fixed_start = 0.38;\n"
  "      const float ceiling = 0.98;\n"
  "      float luma = dot(cogl_color_out.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
  "      float threshold = fixed_start +\n"
  "        (ceiling - fixed_start) * (1.0 - adaptive_strength) * 0.6;\n"
  "      float t = clamp((luma - threshold) /\n"
  "                      max(ceiling - threshold, 0.001), 0.0, 1.0);\n"
  "      float floor_brightness = clamp(u_adaptive_brightness_minimum,\n"
  "                                     0.0, brightness);\n"
  "      adaptive_brightness = brightness -\n"
  "        (brightness - floor_brightness) * adaptive_strength * t;\n"
  "    }\n"
  "  cogl_color_out.rgb *= adaptive_brightness;\n";

/* ---------------------------------------------------------------------------
 * BALANCED variant
 *
 * Single luma tap, eased pow() reduction curve — the default quality.
 * --------------------------------------------------------------------------- */

static const gchar *ab_glsl_balanced =
  "  float adaptive_strength = clamp(u_adaptive_brightness_strength, 0.0, 1.0);\n"
  "  float adaptive_brightness = brightness;\n"
  "  if (u_adaptive_brightness > 0.5 && adaptive_strength > 0.0)\n"
  "    {\n"
  "      const float fixed_start = 0.38;\n"
  "      const float ceiling = 0.98;\n"
  "      const float exponent = 2.6;\n"
  "      float luma = dot(cogl_color_out.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
  "      float threshold = fixed_start +\n"
  "        (ceiling - fixed_start) * (1.0 - adaptive_strength) * 0.6;\n"
  "      float normalized = clamp((luma - threshold) /\n"
  "                               max(ceiling - threshold, 0.001), 0.0, 1.0);\n"
  "      float eased = pow(normalized, exponent);\n"
  "      float floor_brightness = clamp(u_adaptive_brightness_minimum,\n"
  "                                     0.0, brightness);\n"
  "      adaptive_brightness = brightness -\n"
  "        (brightness - floor_brightness) * adaptive_strength * eased;\n"
  "    }\n"
  "  cogl_color_out.rgb *= adaptive_brightness;\n";

/* ---------------------------------------------------------------------------
 * QUALITY variant
 *
 * 5-tap cross neighbourhood luma average + eased pow() curve.
 * Sampling four neighbours (±1.5 px in each axis) gives a better estimate of
 * the local region brightness than a single centre tap, at the cost of four
 * extra texture fetches per fragment.
 * --------------------------------------------------------------------------- */

static const gchar *ab_glsl_quality =
  "  float adaptive_strength = clamp(u_adaptive_brightness_strength, 0.0, 1.0);\n"
  "  float adaptive_brightness = brightness;\n"
  "  if (u_adaptive_brightness > 0.5 && adaptive_strength > 0.0)\n"
  "    {\n"
  "      const float fixed_start = 0.38;\n"
  "      const float ceiling = 0.98;\n"
  "      const float exponent = 2.6;\n"
  "      vec2 uv = cogl_tex_coord_in[0].st;\n"
  "      vec2 d  = vec2(1.5) / max(u_ab_tex_size, vec2(1.0));\n"
  "      vec3 s0 = cogl_color_out.rgb;\n"
  "      vec3 s1 = texture2D(cogl_sampler0,\n"
  "                          clamp(uv + vec2( d.x,  0.0),\n"
  "                                vec2(0.0), vec2(1.0))).rgb;\n"
  "      vec3 s2 = texture2D(cogl_sampler0,\n"
  "                          clamp(uv + vec2(-d.x,  0.0),\n"
  "                                vec2(0.0), vec2(1.0))).rgb;\n"
  "      vec3 s3 = texture2D(cogl_sampler0,\n"
  "                          clamp(uv + vec2( 0.0,  d.y),\n"
  "                                vec2(0.0), vec2(1.0))).rgb;\n"
  "      vec3 s4 = texture2D(cogl_sampler0,\n"
  "                          clamp(uv + vec2( 0.0, -d.y),\n"
  "                                vec2(0.0), vec2(1.0))).rgb;\n"
  "      float luma = dot((s0 + s1 + s2 + s3 + s4) * 0.2,\n"
  "                       vec3(0.2126, 0.7152, 0.0722));\n"
  "      float threshold = fixed_start +\n"
  "        (ceiling - fixed_start) * (1.0 - adaptive_strength) * 0.6;\n"
  "      float normalized = clamp((luma - threshold) /\n"
  "                               max(ceiling - threshold, 0.001), 0.0, 1.0);\n"
  "      float eased = pow(normalized, exponent);\n"
  "      float floor_brightness = clamp(u_adaptive_brightness_minimum,\n"
  "                                     0.0, brightness);\n"
  "      adaptive_brightness = brightness -\n"
  "        (brightness - floor_brightness) * adaptive_strength * eased;\n"
  "    }\n"
  "  cogl_color_out.rgb *= adaptive_brightness;\n";

/* ---------------------------------------------------------------------------
 * Pipeline factory
 * --------------------------------------------------------------------------- */

CoglPipeline *
gb_adaptive_brightness_create_pipeline (CoglPipeline               *base,
                                         GbAdaptiveBrightnessQuality quality)
{
  static CoglPipeline *cached[3] = { NULL, NULL, NULL };

  const gchar *declarations;
  const gchar *body;
  CoglPipeline *template;
  CoglSnippet  *snippet;
  int           idx;

  switch (quality)
    {
    case GB_ADAPTIVE_BRIGHTNESS_QUALITY_PERFORMANCE:
      idx          = 0;
      declarations = ab_declarations_base;
      body         = ab_glsl_performance;
      break;

    case GB_ADAPTIVE_BRIGHTNESS_QUALITY_QUALITY:
      idx          = 2;
      declarations = ab_declarations_quality;
      body         = ab_glsl_quality;
      break;

    case GB_ADAPTIVE_BRIGHTNESS_QUALITY_BALANCED:
    default:
      idx          = 1;
      declarations = ab_declarations_base;
      body         = ab_glsl_balanced;
      break;
    }

  if (G_UNLIKELY (cached[idx] == NULL))
    {
      template = cogl_pipeline_copy (base);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT, declarations, body);
      cogl_pipeline_add_snippet (template, snippet);
      g_object_unref (snippet);

      cached[idx] = template;
    }

  return cogl_pipeline_copy (cached[idx]);
}

/* ---------------------------------------------------------------------------
 * Per-frame uniform application
 * --------------------------------------------------------------------------- */

void
gb_adaptive_brightness_apply (CoglPipeline *pipeline,
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
                               float         tex_height)
{
  CoglColor color;

  cogl_color_init_from_4f (&color,
                           paint_opacity / 255.0f,
                           paint_opacity / 255.0f,
                           paint_opacity / 255.0f,
                           paint_opacity / 255.0f);
  cogl_pipeline_set_color (pipeline, &color);

  if (brightness_uniform > -1)
    cogl_pipeline_set_uniform_1f (pipeline, brightness_uniform, brightness);

  if (adaptive_uniform > -1)
    cogl_pipeline_set_uniform_1f (pipeline, adaptive_uniform,
                                  adaptive ? 1.f : 0.f);

  if (strength_uniform > -1)
    cogl_pipeline_set_uniform_1f (pipeline, strength_uniform, strength);

  if (minimum_uniform > -1)
    cogl_pipeline_set_uniform_1f (pipeline, minimum_uniform, minimum);

  if (tex_size_uniform > -1)
    {
      const float size[2] = { tex_width, tex_height };

      cogl_pipeline_set_uniform_float (pipeline, tex_size_uniform, 2, 1, size);
    }
}
