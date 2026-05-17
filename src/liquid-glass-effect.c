/* liquid-glass-effect.c
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

#include <mtk/mtk.h>

#include "liquid-glass-effect.h"
#include "adaptive-brightness.h"

#include <math.h>

static const gchar *size_glsl_declarations =
  "uniform vec2 u_size;\n"
  "uniform float u_corner_radius;\n";

static const gchar *glass_lookup_glsl_declarations =
  "uniform float u_refraction;\n"
  "uniform float u_depth;\n";

static const gchar *glass_lookup_glsl =
  "  vec2 uv = cogl_tex_coord.xy;\n"
  "  vec2 size = max(u_size, vec2(1.0));\n"
  "  vec2 half_size = max(size * 0.5, vec2(1.0));\n"
  "  float radius = clamp(u_corner_radius,\n"
  "                       0.0,\n"
  "                       max(min(half_size.x, half_size.y) - 0.5, 0.0));\n"
  "\n"
  "  const float GLASS_A = 0.7;\n"
  "  const float GLASS_B = 2.3;\n"
  "  const float GLASS_C = 5.2;\n"
  "  const float M_E = 2.718281828459045;\n"
  "\n"
  "  vec2 pos = uv * size;\n"
  "  vec2 local = (pos - half_size) / half_size;\n"
  "  vec2 quad = abs(pos - half_size) - half_size + radius;\n"
  "  float dist = length(max(quad, vec2(0.0))) - radius;\n"
  "\n"
  "  float depth = clamp(u_depth / 100.0, 0.0, 1.0);\n"
  "  float d = mix(0.05, 0.3, 1.0 - depth);\n"
  "  float power = clamp(u_refraction / 100.0, 0.0, 1.5);\n"
  "  float lens = 1.0 - GLASS_B * pow(GLASS_C * M_E,\n"
  "                                   -d * -dist - GLASS_A);\n"
  "  vec2 refracted = local * pow(max(lens, 0.0001), power);\n"
  "  vec2 refracted_uv = (refracted * half_size + half_size) / size;\n"
  "\n"
  "  cogl_tex_coord.xy = clamp(refracted_uv,\n"
  "                            vec2(3.0) / size,\n"
  "                            vec2(1.0) - vec2(3.0) / size);\n";

static const gchar *glass_glsl_declarations =
  "uniform float u_glow_weight;\n"
  "uniform float u_glow_bias;\n"
  "uniform float u_glow_bevel;\n"
  "uniform float u_glow_smooth;\n";

static const gchar *glass_glsl =
  "  vec2 uv = cogl_tex_coord_in[0].st;\n"
  "  vec2 size = max(u_size, vec2(1.0));\n"
  "  vec2 half_size = max(size * 0.5, vec2(1.0));\n"
  "  float radius = clamp(u_corner_radius,\n"
  "                       0.0,\n"
  "                       max(min(half_size.x, half_size.y) - 0.5, 0.0));\n"
  "\n"
  "  vec2 pos = uv * size;\n"
  "  vec2 local = (pos - half_size) / half_size;\n"
  "  vec2 quad = abs(pos - half_size) - half_size + radius;\n"
  "  float dist = length(max(quad, vec2(0.0))) - radius;\n"
  "  float mask = 1.0 - smoothstep(0.0, 1.0, dist);\n"
  "\n"
  "  float angle_glow = sin(atan(local.y, local.x) - 0.5);\n"
  "  float edge_glow = smoothstep(max(u_glow_bevel, 1.0),\n"
  "                               -u_glow_smooth,\n"
  "                               -dist);\n"
  "  float glow_mul = angle_glow * u_glow_weight * edge_glow +\n"
  "                   1.0 + u_glow_bias;\n"
  "\n"
  "  cogl_color_out.rgb *= glow_mul;\n"
  "  cogl_color_out.rgb *= mask;\n"
  "  cogl_color_out.a *= mask;\n";

#define MIN_DOWNSCALE_SIZE 256.f
#define MAX_RADIUS 12.f
#define MAX_EFFECT_RADIUS 200

typedef enum
{
  ACTOR_PAINTED = 1 << 0,
  BLUR_APPLIED = 1 << 1,
} CacheFlags;

typedef struct
{
  CoglFramebuffer *framebuffer;
  CoglPipeline *pipeline;
  CoglTexture *texture;
} FramebufferData;

struct _GbLiquidGlassEffect
{
  ClutterEffect parent_instance;

  ClutterActor *actor;

  float tex_width;
  float tex_height;

  /* The cached contents */
  FramebufferData actor_fb;
  CacheFlags cache_flags;

  FramebufferData background_fb;
  FramebufferData brightness_fb;
  int brightness_uniform;
  int adaptive_brightness_uniform;
  int adaptive_brightness_strength_uniform;
  int adaptive_brightness_minimum_uniform;
  int adaptive_brightness_tex_size_uniform;

  FramebufferData mask_fb;
  int corner_radius_uniform;
  int mask_size_uniform;
  int glow_weight_uniform;
  int glow_bias_uniform;
  int glow_bevel_uniform;
  int glow_smooth_uniform;
  int refraction_uniform;
  int depth_uniform;

  GbBlurMode mode;
  float downscale_factor;
  float brightness;
  gboolean adaptive_brightness;
  float adaptive_brightness_strength;
  float adaptive_brightness_minimum;
  GbAdaptiveBrightnessQuality adaptive_brightness_quality;
  int radius;

  float corner_radius;
  float glow_weight;
  float glow_bias;
  float glow_bevel;
  float glow_smooth;
  float refraction;
  float depth;
};

G_DEFINE_TYPE (GbLiquidGlassEffect, gb_liquid_glass_effect, CLUTTER_TYPE_EFFECT)

enum {
  PROP_0,
  PROP_RADIUS,
  PROP_BRIGHTNESS,
  PROP_ADAPTIVE_BRIGHTNESS,
  PROP_ADAPTIVE_BRIGHTNESS_STRENGTH,
  PROP_ADAPTIVE_BRIGHTNESS_MINIMUM,
  PROP_ADAPTIVE_BRIGHTNESS_QUALITY,
  PROP_MODE,
  PROP_CORNER_RADIUS,
  PROP_HIGHLIGHT,
  PROP_GLOW_WEIGHT,
  PROP_GLOW_BIAS,
  PROP_GLOW_BEVEL,
  PROP_GLOW_SMOOTH,
  PROP_REFRACTION,
  PROP_DEPTH,
  PROP_DEBUG_LOGGING,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static CoglPipeline *
create_base_pipeline (void)
{
  static CoglPipeline *base_pipeline = NULL;

  if (G_UNLIKELY (base_pipeline == NULL))
    {
      ClutterBackend *backend = clutter_get_default_backend ();
      CoglContext *ctx;

      if (!backend)
        return NULL;

      ctx = clutter_backend_get_cogl_context (backend);
      if (!ctx)
        return NULL;

      base_pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_null_texture (base_pipeline, 0);
      cogl_pipeline_set_layer_filters (base_pipeline,
                                       0,
                                       COGL_PIPELINE_FILTER_LINEAR,
                                       COGL_PIPELINE_FILTER_LINEAR);
      cogl_pipeline_set_layer_wrap_mode (base_pipeline,
                                         0,
                                         COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    }

  return cogl_pipeline_copy (base_pipeline);
}

static void
rebuild_brightness_pipeline (GbLiquidGlassEffect *self)
{
  g_autoptr (CoglPipeline) base = create_base_pipeline ();

  g_clear_object (&self->brightness_fb.pipeline);

  if (!base)
    {
      self->brightness_uniform = -1;
      self->adaptive_brightness_uniform = -1;
      self->adaptive_brightness_strength_uniform = -1;
      self->adaptive_brightness_minimum_uniform = -1;
      self->adaptive_brightness_tex_size_uniform = -1;
      return;
    }

  self->brightness_fb.pipeline =
    gb_adaptive_brightness_create_pipeline (base, self->adaptive_brightness_quality);

  self->brightness_uniform =
    cogl_pipeline_get_uniform_location (self->brightness_fb.pipeline, "brightness");
  self->adaptive_brightness_uniform =
    cogl_pipeline_get_uniform_location (self->brightness_fb.pipeline,
                                        "u_adaptive_brightness");
  self->adaptive_brightness_strength_uniform =
    cogl_pipeline_get_uniform_location (self->brightness_fb.pipeline,
                                        "u_adaptive_brightness_strength");
  self->adaptive_brightness_minimum_uniform =
    cogl_pipeline_get_uniform_location (self->brightness_fb.pipeline,
                                        "u_adaptive_brightness_minimum");
  self->adaptive_brightness_tex_size_uniform =
    cogl_pipeline_get_uniform_location (self->brightness_fb.pipeline,
                                        "u_ab_tex_size");
}

static CoglPipeline *
create_mask_pipeline (void)
{
  static CoglPipeline *mask_pipeline = NULL;

  if (G_UNLIKELY (mask_pipeline == NULL))
    {
      CoglSnippet *fragment_snippet;
      CoglSnippet *lookup_snippet;

      mask_pipeline = create_base_pipeline ();
      if (!mask_pipeline)
        return NULL;

      fragment_snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT_GLOBALS,
                                           size_glsl_declarations,
                                           NULL);
      cogl_pipeline_add_snippet (mask_pipeline, fragment_snippet);
      g_object_unref (fragment_snippet);

      lookup_snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                                         glass_lookup_glsl_declarations,
                                         NULL);
      cogl_snippet_set_pre (lookup_snippet, glass_lookup_glsl);
      cogl_pipeline_add_layer_snippet (mask_pipeline, 0, lookup_snippet);
      g_object_unref (lookup_snippet);

      fragment_snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                           glass_glsl_declarations,
                                           glass_glsl);
      cogl_pipeline_add_snippet (mask_pipeline, fragment_snippet);
      g_object_unref (fragment_snippet);
    }

  return cogl_pipeline_copy (mask_pipeline);
}

static void
clear_framebuffer_data (FramebufferData *fb_data)
{
  if (fb_data->pipeline)
    cogl_pipeline_set_layer_null_texture (fb_data->pipeline, 0);

  g_clear_object (&fb_data->framebuffer);
  g_clear_object (&fb_data->texture);
}

static gboolean
is_valid_dimension (float value)
{
  return isfinite (value) && value >= 1.f && value <= G_MAXUINT;
}

static float
sanitize_float_property (float value,
                         float min,
                         float max,
                         float fallback)
{
  if (!isfinite (value))
    return fallback;

  return CLAMP (value, min, max);
}

static gboolean
is_valid_mode (GbBlurMode mode)
{
  return mode == GB_BLUR_MODE_ACTOR || mode == GB_BLUR_MODE_BACKGROUND;
}

static void
update_brightness (GbLiquidGlassEffect *self,
                   uint8_t              paint_opacity)
{
  gb_adaptive_brightness_apply (self->brightness_fb.pipeline,
                                self->brightness_uniform,
                                self->adaptive_brightness_uniform,
                                self->adaptive_brightness_strength_uniform,
                                self->adaptive_brightness_minimum_uniform,
                                self->adaptive_brightness_tex_size_uniform,
                                paint_opacity,
                                self->brightness,
                                self->adaptive_brightness,
                                self->adaptive_brightness_strength,
                                self->adaptive_brightness_minimum,
                                self->tex_width / self->downscale_factor,
                                self->tex_height / self->downscale_factor);
}

static void
update_mask_uniforms (GbLiquidGlassEffect *self,
                      float                width,
                      float                height,
                      float                corner_radius)
{
  if (!self->mask_fb.pipeline)
    return;

  if (self->corner_radius_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->mask_fb.pipeline,
                                  self->corner_radius_uniform,
                                  corner_radius);

  if (self->mask_size_uniform > -1)
    {
      const float size[2] = { width, height };

      cogl_pipeline_set_uniform_float (self->mask_fb.pipeline,
                                       self->mask_size_uniform,
                                       2, 1, size);
    }

  if (self->glow_weight_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->mask_fb.pipeline,
                                  self->glow_weight_uniform,
                                  self->glow_weight / 10.f);

  if (self->glow_bias_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->mask_fb.pipeline,
                                  self->glow_bias_uniform,
                                  self->glow_bias / 10.f);

  if (self->glow_bevel_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->mask_fb.pipeline,
                                  self->glow_bevel_uniform,
                                  self->glow_bevel);

  if (self->glow_smooth_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->mask_fb.pipeline,
                                  self->glow_smooth_uniform,
                                  self->glow_smooth);

  if (self->refraction_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->mask_fb.pipeline,
                                  self->refraction_uniform,
                                  self->refraction);

  if (self->depth_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->mask_fb.pipeline,
                                  self->depth_uniform,
                                  self->depth);
}

static void
setup_projection_matrix (CoglFramebuffer *framebuffer,
                         float            width,
                         float            height)
{
  graphene_matrix_t projection;

  graphene_matrix_init_translate (&projection,
                                  &GRAPHENE_POINT3D_INIT (-width / 2.0,
                                                          -height / 2.0,
                                                          0.f));
  graphene_matrix_scale (&projection, 2.0 / width, -2.0 / height, 1.f);

  cogl_framebuffer_set_projection_matrix (framebuffer, &projection);
}

static gboolean
update_fbo (FramebufferData *data,
            float            width,
            float            height,
            float            downscale_factor)
{
  ClutterBackend *backend;
  CoglContext *ctx;
  float new_width;
  float new_height;

  if (!data->pipeline ||
      !is_valid_dimension (width) ||
      !is_valid_dimension (height) ||
      !isfinite (downscale_factor) ||
      downscale_factor < 1.f)
    return FALSE;

  new_width = floorf (width / downscale_factor);
  new_height = floorf (height / downscale_factor);

  if (!is_valid_dimension (new_width) || !is_valid_dimension (new_height))
    return FALSE;

  backend = clutter_get_default_backend ();
  if (!backend)
    return FALSE;

  ctx = clutter_backend_get_cogl_context (backend);
  if (!ctx)
    return FALSE;

  clear_framebuffer_data (data);

  data->texture = cogl_texture_2d_new_with_size (ctx, new_width, new_height);
  if (!data->texture)
    return FALSE;

  cogl_pipeline_set_layer_texture (data->pipeline, 0, data->texture);

  data->framebuffer =
    COGL_FRAMEBUFFER (cogl_offscreen_new_with_texture (data->texture));
  if (!data->framebuffer)
    {
      g_warning ("%s: Unable to create an Offscreen buffer", G_STRLOC);
      clear_framebuffer_data (data);
      return FALSE;
    }

  setup_projection_matrix (data->framebuffer, new_width, new_height);

  return TRUE;
}

static gboolean
update_actor_fbo (GbLiquidGlassEffect *self,
                  float                width,
                  float                height,
                  float                downscale_factor)
{
  if (self->tex_width == width &&
      self->tex_height == height &&
      self->downscale_factor == downscale_factor &&
      self->actor_fb.framebuffer)
    {
      return TRUE;
    }

  self->cache_flags &= ~ACTOR_PAINTED;

  return update_fbo (&self->actor_fb, width, height, downscale_factor);
}

static gboolean
update_brightness_fbo (GbLiquidGlassEffect *self,
                       float                width,
                       float                height,
                       float                downscale_factor)
{
  if (self->tex_width == width &&
      self->tex_height == height &&
      self->downscale_factor == downscale_factor &&
      self->brightness_fb.framebuffer)
    {
      return TRUE;
    }

  return update_fbo (&self->brightness_fb, width, height, downscale_factor);
}

static gboolean
update_background_fbo (GbLiquidGlassEffect *self,
                       float                width,
                       float                height)
{
  if (self->tex_width == width &&
      self->tex_height == height &&
      self->background_fb.framebuffer)
    {
      return TRUE;
    }

  return update_fbo (&self->background_fb, width, height, 1.0);
}

static gboolean
update_mask_fbo (GbLiquidGlassEffect *self,
                 float                width,
                 float                height,
                 float                downscale_factor)
{
  if (self->tex_width == width &&
      self->tex_height == height &&
      self->downscale_factor == downscale_factor &&
      self->mask_fb.framebuffer)
    {
      return TRUE;
    }

  return update_fbo (&self->mask_fb, width, height, downscale_factor);
}

static float
calculate_downscale_factor (float width,
                            float height,
                            float radius)
{
  float downscale_factor = 1.0;
  float scaled_width = width;
  float scaled_height = height;
  float scaled_radius = radius;

  /* This is the algorithm used by Firefox; keep downscaling until either the
   * blur radius is lower than the threshold, or the downscaled texture is too
   * small.
   */
  while (scaled_radius > MAX_RADIUS &&
         scaled_width > MIN_DOWNSCALE_SIZE &&
         scaled_height > MIN_DOWNSCALE_SIZE)
    {
      downscale_factor *= 2.f;

      scaled_width = width / downscale_factor;
      scaled_height = height / downscale_factor;
      scaled_radius = radius / downscale_factor;
    }

  return downscale_factor;
}

static void
gb_liquid_glass_effect_set_actor (ClutterActorMeta *meta,
                                  ClutterActor     *actor)
{
  GbLiquidGlassEffect *self = GB_LIQUID_GLASS_EFFECT (meta);
  ClutterActorMetaClass *meta_class;

  meta_class = CLUTTER_ACTOR_META_CLASS (gb_liquid_glass_effect_parent_class);
  meta_class->set_actor (meta, actor);

  /* clear out the previous state */
  clear_framebuffer_data (&self->actor_fb);
  clear_framebuffer_data (&self->background_fb);
  clear_framebuffer_data (&self->brightness_fb);
  clear_framebuffer_data (&self->mask_fb);
  self->cache_flags = 0;

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  self->actor = clutter_actor_meta_get_actor (meta);

  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] set-actor actor=%p name=%s mode=%d radius=%d",
                 (void *) self,
                 (void *) self->actor,
                 self->actor ? clutter_actor_get_name (self->actor) : "(none)",
                 self->mode,
                 self->radius);
}

static void
update_actor_box (GbLiquidGlassEffect *self,
                  ClutterPaintContext *paint_context,
                  ClutterActorBox     *source_actor_box)
{
  ClutterStageView *stage_view;
  float box_scale_factor = 1.0f;
  float origin_x, origin_y;
  float width, height;

  switch (self->mode)
    {
    case GB_BLUR_MODE_ACTOR:
      clutter_actor_get_allocation_box (self->actor, source_actor_box);
      break;

    case GB_BLUR_MODE_BACKGROUND:
      stage_view = clutter_paint_context_get_stage_view (paint_context);

      clutter_actor_get_transformed_position (self->actor, &origin_x, &origin_y);
      clutter_actor_get_size (self->actor, &width, &height);

      if (stage_view)
        {
          MtkRectangle stage_view_layout;

          box_scale_factor = clutter_stage_view_get_scale (stage_view);
          clutter_stage_view_get_layout (stage_view, &stage_view_layout);

          origin_x -= stage_view_layout.x;
          origin_y -= stage_view_layout.y;
        }
      else
        {
          /* If we're drawing off stage, just assume scale = 1, this won't work
           * with stage-view scaling though.
           */
        }

      clutter_actor_box_set_origin (source_actor_box, origin_x, origin_y);
      clutter_actor_box_set_size (source_actor_box, width, height);

      clutter_actor_box_scale (source_actor_box, box_scale_factor);
      break;
    }

  clutter_actor_box_clamp_to_pixel (source_actor_box);
}

static void
add_paint_rectangle (ClutterPaintNode *node,
                     float             width,
                     float             height)
{
  clutter_paint_node_add_rectangle (node,
                                    &(ClutterActorBox) {
                                      0.f, 0.f,
                                      width, height,
                                    });
}

static void
add_blurred_pipeline (GbLiquidGlassEffect *self,
                      ClutterPaintNode *node,
                      uint8_t           paint_opacity)
{
  g_autoptr (ClutterPaintNode) pipeline_node = NULL;
  float width, height;

  /* Use the untransformed actor size here, since the framebuffer itself already
   * has the actor transform matrix applied.
   */
  clutter_actor_get_size (self->actor, &width, &height);

  update_brightness (self, paint_opacity);
  update_mask_uniforms (self, width, height, self->corner_radius);

  pipeline_node = clutter_pipeline_node_new (self->mask_fb.pipeline);
  clutter_paint_node_set_static_name (pipeline_node, "GbLiquidGlassEffect (final)");
  clutter_paint_node_add_child (node, pipeline_node);

  add_paint_rectangle (pipeline_node, width, height);
}

static ClutterPaintNode *
create_blur_nodes (GbLiquidGlassEffect *self,
                   ClutterPaintNode *node,
                   uint8_t           paint_opacity)
{
  g_autoptr (ClutterPaintNode) brightness_node = NULL;
  g_autoptr (ClutterPaintNode) blur_node = NULL;
  g_autoptr (ClutterPaintNode) mask_node = NULL;
  float scaled_width = self->tex_width / self->downscale_factor;
  float scaled_height = self->tex_height / self->downscale_factor;
  float width;
  float height;

  clutter_actor_get_size (self->actor, &width, &height);

  update_mask_uniforms (self, width, height, self->corner_radius);
  mask_node = clutter_layer_node_new_to_framebuffer (self->mask_fb.framebuffer,
                                                     self->mask_fb.pipeline);
  clutter_paint_node_set_static_name (mask_node, "ShellLiquidGlassEffect (mask)");
  clutter_paint_node_add_child (node, mask_node);
  add_paint_rectangle (mask_node, width, height);

  update_brightness (self, paint_opacity);
  brightness_node =
    clutter_layer_node_new_to_framebuffer (self->brightness_fb.framebuffer,
                                           self->brightness_fb.pipeline);
  clutter_paint_node_set_static_name (brightness_node, "ShellLiquidGlassEffect (brightness)");
  clutter_paint_node_add_child (mask_node, brightness_node);
  add_paint_rectangle (brightness_node, scaled_width, scaled_height);

  blur_node = clutter_blur_node_new (scaled_width,
                                     scaled_height,
                                     self->radius / self->downscale_factor);
  clutter_paint_node_set_static_name (blur_node, "ShellLiquidGlassEffect (blur)");
  clutter_paint_node_add_child (brightness_node, blur_node);
  add_paint_rectangle (blur_node, scaled_width, scaled_height);

  self->cache_flags |= BLUR_APPLIED;

  return g_steal_pointer (&blur_node);
}

static void
paint_background (GbLiquidGlassEffect *self,
                  ClutterPaintNode    *node,
                  ClutterPaintContext *paint_context,
                  ClutterActorBox     *source_actor_box)
{
  g_autoptr (ClutterPaintNode) background_node = NULL;
  g_autoptr (ClutterPaintNode) blit_node = NULL;
  CoglFramebuffer *src;
  float transformed_x;
  float transformed_y;
  float transformed_width;
  float transformed_height;

  clutter_actor_box_get_origin (source_actor_box,
                                &transformed_x,
                                &transformed_y);
  clutter_actor_box_get_size (source_actor_box,
                              &transformed_width,
                              &transformed_height);

  /* Background layer node */
  background_node =
    clutter_layer_node_new_to_framebuffer (self->background_fb.framebuffer,
                                           self->background_fb.pipeline);
  clutter_paint_node_set_static_name (background_node, "GbLiquidGlassEffect (background)");
  clutter_paint_node_add_child (node, background_node);
  add_paint_rectangle (background_node,
                       self->tex_width / self->downscale_factor,
                       self->tex_height / self->downscale_factor);

  /* Blit node */
  src = clutter_paint_context_get_framebuffer (paint_context);
  blit_node = clutter_blit_node_new (src);
  clutter_paint_node_set_static_name (blit_node, "GbLiquidGlassEffect (blit)");
  clutter_paint_node_add_child (background_node, blit_node);
  clutter_blit_node_add_blit_rectangle (CLUTTER_BLIT_NODE (blit_node),
                                        transformed_x,
                                        transformed_y,
                                        0, 0,
                                        transformed_width,
                                        transformed_height);
}

static gboolean
update_framebuffers (GbLiquidGlassEffect *self,
                     ClutterActorBox     *source_actor_box,
                     gboolean            *reallocated)
{
  gboolean updated = FALSE;
  gboolean already_ready;
  float downscale_factor;
  float height = -1;
  float width = -1;

  if (reallocated)
    *reallocated = FALSE;

  clutter_actor_box_get_size (source_actor_box, &width, &height);

  if (!is_valid_dimension (width) || !is_valid_dimension (height))
    {
      GB_BLUR_DEBUG ("LiquidGlassEffect[%p] framebuffers skipped invalid-size w=%.1f h=%.1f mode=%d radius=%d",
                     (void *) self, width, height, self->mode, self->radius);
    return FALSE;
    }

  downscale_factor = calculate_downscale_factor (width, height, self->radius);
  already_ready = self->tex_width == width &&
                  self->tex_height == height &&
                  self->downscale_factor == downscale_factor &&
                  self->brightness_fb.framebuffer &&
                  self->mask_fb.framebuffer &&
                  (self->mode != GB_BLUR_MODE_ACTOR ||
                   self->actor_fb.framebuffer) &&
                  (self->mode != GB_BLUR_MODE_BACKGROUND ||
                   self->background_fb.framebuffer);

  if (self->mode == GB_BLUR_MODE_ACTOR)
    updated = update_actor_fbo (self, width, height, downscale_factor);
  else
    updated = TRUE;

  updated = updated &&
            update_brightness_fbo (self, width, height, downscale_factor) &&
            update_mask_fbo (self, width, height, downscale_factor);

  if (self->mode == GB_BLUR_MODE_BACKGROUND)
    updated = updated && update_background_fbo (self, width, height);

  if (updated)
    {
      self->tex_width = width;
      self->tex_height = height;
      self->downscale_factor = downscale_factor;
    }

  if (reallocated)
    *reallocated = updated && !already_ready;

  if (!updated || !already_ready)
    GB_BLUR_DEBUG ("LiquidGlassEffect[%p] framebuffers %s w=%.1f h=%.1f downscale=%.1f mode=%d radius=%d adaptive=%d quality=%d",
                   (void *) self,
                   updated ? "allocated" : "failed",
                   width,
                   height,
                   downscale_factor,
                   self->mode,
                   self->radius,
                   self->adaptive_brightness,
                   self->adaptive_brightness_quality);

  return updated;
}

static void
add_actor_node (GbLiquidGlassEffect *self,
                ClutterPaintNode *node,
                int               opacity)
{
  g_autoptr (ClutterPaintNode) actor_node = NULL;

  actor_node = clutter_actor_node_new (self->actor, opacity);
  clutter_paint_node_add_child (node, actor_node);
}

static void
paint_actor_offscreen (GbLiquidGlassEffect     *self,
                       ClutterPaintNode        *node,
                       ClutterEffectPaintFlags  flags)
{
  gboolean actor_dirty;

  actor_dirty = (flags & CLUTTER_EFFECT_PAINT_ACTOR_DIRTY) != 0;

  /* The actor offscreen framebuffer is updated already */
  if (actor_dirty || !(self->cache_flags & ACTOR_PAINTED))
    {
      g_autoptr (ClutterPaintNode) transform_node = NULL;
      g_autoptr (ClutterPaintNode) layer_node = NULL;
      graphene_matrix_t transform;

      /* Layer node */
      layer_node =
        clutter_layer_node_new_to_framebuffer (self->actor_fb.framebuffer,
                                               self->actor_fb.pipeline);
      clutter_paint_node_set_static_name (layer_node, "GbLiquidGlassEffect (actor offscreen)");
      clutter_paint_node_add_child (node, layer_node);
      add_paint_rectangle (layer_node,
                           self->tex_width / self->downscale_factor,
                           self->tex_height / self->downscale_factor);

      /* Transform node */
      graphene_matrix_init_scale (&transform,
                                  1.f / self->downscale_factor,
                                  1.f / self->downscale_factor,
                                  1.f);
      transform_node = clutter_transform_node_new (&transform);
      clutter_paint_node_set_static_name (transform_node, "GbLiquidGlassEffect (downscale)");
      clutter_paint_node_add_child (layer_node, transform_node);

      /* Actor node */
      add_actor_node (self, transform_node, 255);

      self->cache_flags |= ACTOR_PAINTED;
    }
  else
    {
      g_autoptr (ClutterPaintNode) pipeline_node = NULL;

      pipeline_node = clutter_pipeline_node_new (self->actor_fb.pipeline);
      clutter_paint_node_set_static_name (pipeline_node,
                                          "GbLiquidGlassEffect (actor texture)");
      clutter_paint_node_add_child (node, pipeline_node);
      add_paint_rectangle (pipeline_node,
                           self->tex_width / self->downscale_factor,
                           self->tex_height / self->downscale_factor);
    }
}

static gboolean
needs_repaint (GbLiquidGlassEffect     *self,
               ClutterEffectPaintFlags  flags)
{
  gboolean actor_cached;
  gboolean blur_cached;
  gboolean actor_dirty;

  actor_dirty = (flags & CLUTTER_EFFECT_PAINT_ACTOR_DIRTY) != 0;
  blur_cached = (self->cache_flags & BLUR_APPLIED) != 0;
  actor_cached = (self->cache_flags & ACTOR_PAINTED) != 0;

  switch (self->mode)
    {
    case GB_BLUR_MODE_ACTOR:
      return actor_dirty || !blur_cached || !actor_cached;

    case GB_BLUR_MODE_BACKGROUND:
      return TRUE;
    }

  return TRUE;
}

static void
gb_liquid_glass_effect_paint_node (ClutterEffect           *effect,
                                   ClutterPaintNode        *node,
                                   ClutterPaintContext     *paint_context,
                                   ClutterEffectPaintFlags  flags)
{
  GbLiquidGlassEffect *self = GB_LIQUID_GLASS_EFFECT (effect);
  uint8_t paint_opacity;
  gint64 start_us = 0;

  if (G_UNLIKELY (gb_blur_debug_logging_enabled))
    start_us = g_get_monotonic_time ();

  if (!self->actor)
    return;

  if (self->radius > 0)
    {
      g_autoptr (ClutterPaintNode) blur_node = NULL;

      switch (self->mode)
        {
        case GB_BLUR_MODE_ACTOR:
          paint_opacity = clutter_actor_get_paint_opacity (self->actor);
          break;

        case GB_BLUR_MODE_BACKGROUND:
          paint_opacity = 255;
          break;

        default:
          goto fail;
        }

      gboolean repaint_required = needs_repaint (self, flags);
      if (repaint_required)
        {
          ClutterActorBox source_actor_box;
          gboolean reallocated = FALSE;
          float source_width;
          float source_height;

          update_actor_box (self, paint_context, &source_actor_box);
          clutter_actor_box_get_size (&source_actor_box, &source_width, &source_height);

          /* Failing to create or update the offscreen framebuffers prevents
           * the entire effect to be applied.
           */
          if (!update_framebuffers (self, &source_actor_box, &reallocated))
            goto fail;

          blur_node = create_blur_nodes (self, node, paint_opacity);

          switch (self->mode)
            {
            case GB_BLUR_MODE_ACTOR:
              paint_actor_offscreen (self, blur_node, flags);
              break;

            case GB_BLUR_MODE_BACKGROUND:
              paint_background (self, blur_node, paint_context, &source_actor_box);
              break;
            }

          if (G_UNLIKELY (gb_blur_debug_logging_enabled))
            {
              double elapsed_ms = (g_get_monotonic_time () - start_us) / 1000.0;

              if (reallocated || elapsed_ms >= 2.0)
                GB_BLUR_DEBUG ("LiquidGlassEffect[%p] paint repaint elapsed=%.3fms actor=%p name=%s mode=%d source=%.1fx%.1f tex=%.1fx%.1f downscale=%.1f radius=%d brightness=%.3f adaptive=%d quality=%d refraction=%.1f depth=%.1f flags=0x%x",
                               (void *) self,
                               elapsed_ms,
                               (void *) self->actor,
                               clutter_actor_get_name (self->actor),
                               self->mode,
                               source_width,
                               source_height,
                               self->tex_width,
                               self->tex_height,
                               self->downscale_factor,
                               self->radius,
                               self->brightness,
                               self->adaptive_brightness,
                               self->adaptive_brightness_quality,
                               self->refraction,
                               self->depth,
                               flags);
            }
        }
      else
        {
          /* Use the cached pipeline if no repaint is needed */
          add_blurred_pipeline (self, node, paint_opacity);
          if (G_UNLIKELY (gb_blur_debug_logging_enabled))
            {
              double elapsed_ms = (g_get_monotonic_time () - start_us) / 1000.0;

              if (elapsed_ms >= 2.0)
                GB_BLUR_DEBUG ("LiquidGlassEffect[%p] paint cached elapsed=%.3fms actor=%p name=%s mode=%d tex=%.1fx%.1f downscale=%.1f radius=%d flags=0x%x",
                               (void *) self,
                               elapsed_ms,
                               (void *) self->actor,
                               clutter_actor_get_name (self->actor),
                               self->mode,
                               self->tex_width,
                               self->tex_height,
                               self->downscale_factor,
                               self->radius,
                               flags);
            }
        }

      /* Background blur needs to paint the actor after painting the blurred
       * background.
       */
      switch (self->mode)
        {
        case GB_BLUR_MODE_ACTOR:
          break;

        case GB_BLUR_MODE_BACKGROUND:
          add_actor_node (self, node, -1);
          break;
        }

      return;
    }

fail:
  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] paint fallback elapsed=%.3fms actor=%p name=%s mode=%d radius=%d flags=0x%x",
                 (void *) self,
                 start_us ? (g_get_monotonic_time () - start_us) / 1000.0 : 0.0,
                 (void *) self->actor,
                 self->actor ? clutter_actor_get_name (self->actor) : "(none)",
                 self->mode,
                 self->radius,
                 flags);
  /* When no blur is applied, or the offscreen framebuffers
   * couldn't be created, fallback to simply painting the actor.
   */
  add_actor_node (self, node, -1);
}

static void
gb_liquid_glass_effect_finalize (GObject *object)
{
  GbLiquidGlassEffect *self = (GbLiquidGlassEffect *)object;

  clear_framebuffer_data (&self->actor_fb);
  clear_framebuffer_data (&self->background_fb);
  clear_framebuffer_data (&self->brightness_fb);
  clear_framebuffer_data (&self->mask_fb);

  g_clear_object (&self->actor_fb.pipeline);
  g_clear_object (&self->background_fb.pipeline);
  g_clear_object (&self->brightness_fb.pipeline);
  g_clear_object (&self->mask_fb.pipeline);

  G_OBJECT_CLASS (gb_liquid_glass_effect_parent_class)->finalize (object);
}

static void
gb_liquid_glass_effect_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbLiquidGlassEffect *self = GB_LIQUID_GLASS_EFFECT (object);

  switch (prop_id)
    {
    case PROP_RADIUS:
      g_value_set_int (value, self->radius);
      break;

    case PROP_BRIGHTNESS:
      g_value_set_float (value, self->brightness);
      break;

    case PROP_ADAPTIVE_BRIGHTNESS:
      g_value_set_boolean (value, self->adaptive_brightness);
      break;

    case PROP_ADAPTIVE_BRIGHTNESS_STRENGTH:
      g_value_set_float (value, self->adaptive_brightness_strength);
      break;

    case PROP_ADAPTIVE_BRIGHTNESS_MINIMUM:
      g_value_set_float (value, self->adaptive_brightness_minimum);
      break;

    case PROP_ADAPTIVE_BRIGHTNESS_QUALITY:
      g_value_set_int (value, (int) self->adaptive_brightness_quality);
      break;

    case PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;

    case PROP_CORNER_RADIUS:
      g_value_set_float (value, self->corner_radius);
      break;

    case PROP_HIGHLIGHT:
      g_value_set_float (value, self->glow_weight / 42.85714f);
      break;

    case PROP_GLOW_WEIGHT:
      g_value_set_float (value, self->glow_weight);
      break;

    case PROP_GLOW_BIAS:
      g_value_set_float (value, self->glow_bias);
      break;

    case PROP_GLOW_BEVEL:
      g_value_set_float (value, self->glow_bevel);
      break;

    case PROP_GLOW_SMOOTH:
      g_value_set_float (value, self->glow_smooth);
      break;

    case PROP_REFRACTION:
      g_value_set_float (value, self->refraction);
      break;

    case PROP_DEPTH:
      g_value_set_float (value, self->depth);
      break;

    case PROP_DEBUG_LOGGING:
      g_value_set_boolean (value, gb_get_debug_logging_enabled ());
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_liquid_glass_effect_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbLiquidGlassEffect *self = GB_LIQUID_GLASS_EFFECT (object);

  switch (prop_id)
    {
    case PROP_RADIUS:
      gb_liquid_glass_effect_set_radius (self, g_value_get_int (value));
      break;

    case PROP_BRIGHTNESS:
      gb_liquid_glass_effect_set_brightness (self, g_value_get_float (value));
      break;

    case PROP_ADAPTIVE_BRIGHTNESS:
      gb_liquid_glass_effect_set_adaptive_brightness (self, g_value_get_boolean (value));
      break;

    case PROP_ADAPTIVE_BRIGHTNESS_STRENGTH:
      gb_liquid_glass_effect_set_adaptive_brightness_strength (self, g_value_get_float (value));
      break;

    case PROP_ADAPTIVE_BRIGHTNESS_MINIMUM:
      gb_liquid_glass_effect_set_adaptive_brightness_minimum (self, g_value_get_float (value));
      break;

    case PROP_ADAPTIVE_BRIGHTNESS_QUALITY:
      gb_liquid_glass_effect_set_adaptive_brightness_quality (
        self, (GbAdaptiveBrightnessQuality) g_value_get_int (value));
      break;

    case PROP_MODE:
      gb_liquid_glass_effect_set_mode (self, g_value_get_enum (value));
      break;

    case PROP_CORNER_RADIUS:
      gb_liquid_glass_effect_set_corner_radius (self, g_value_get_float (value));
      break;

    case PROP_HIGHLIGHT:
      gb_liquid_glass_effect_set_highlight (self, g_value_get_float (value));
      break;

    case PROP_GLOW_WEIGHT:
      gb_liquid_glass_effect_set_glow_weight (self, g_value_get_float (value));
      break;

    case PROP_GLOW_BIAS:
      gb_liquid_glass_effect_set_glow_bias (self, g_value_get_float (value));
      break;

    case PROP_GLOW_BEVEL:
      gb_liquid_glass_effect_set_glow_bevel (self, g_value_get_float (value));
      break;

    case PROP_GLOW_SMOOTH:
      gb_liquid_glass_effect_set_glow_smooth (self, g_value_get_float (value));
      break;

    case PROP_REFRACTION:
      gb_liquid_glass_effect_set_refraction (self, g_value_get_float (value));
      break;

    case PROP_DEPTH:
      gb_liquid_glass_effect_set_depth (self, g_value_get_float (value));
      break;

    case PROP_DEBUG_LOGGING:
      gb_set_debug_logging_enabled (g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_liquid_glass_effect_class_init (GbLiquidGlassEffectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);

  object_class->finalize = gb_liquid_glass_effect_finalize;
  object_class->get_property = gb_liquid_glass_effect_get_property;
  object_class->set_property = gb_liquid_glass_effect_set_property;

  meta_class->set_actor = gb_liquid_glass_effect_set_actor;

  effect_class->paint_node = gb_liquid_glass_effect_paint_node;

  properties[PROP_RADIUS] =
    g_param_spec_int ("radius", NULL, NULL,
                      0, MAX_EFFECT_RADIUS, 0,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_BRIGHTNESS] =
    g_param_spec_float ("brightness", NULL, NULL,
                        0.f, 1.f, 1.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ADAPTIVE_BRIGHTNESS] =
    g_param_spec_boolean ("adaptive-brightness", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ADAPTIVE_BRIGHTNESS_STRENGTH] =
    g_param_spec_float ("adaptive-brightness-strength", NULL, NULL,
                        0.f, 1.f, 0.75f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ADAPTIVE_BRIGHTNESS_MINIMUM] =
    g_param_spec_float ("adaptive-brightness-minimum", NULL, NULL,
                        0.f, 1.f, 0.28f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ADAPTIVE_BRIGHTNESS_QUALITY] =
    g_param_spec_int ("adaptive-brightness-quality", NULL, NULL,
                      GB_ADAPTIVE_BRIGHTNESS_QUALITY_PERFORMANCE,
                      GB_ADAPTIVE_BRIGHTNESS_QUALITY_QUALITY,
                      GB_ADAPTIVE_BRIGHTNESS_QUALITY_BALANCED,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_MODE] =
    g_param_spec_enum ("mode", NULL, NULL,
                       GB_TYPE_BLUR_MODE,
                       GB_BLUR_MODE_ACTOR,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_CORNER_RADIUS] =
    g_param_spec_float ("corner-radius", NULL, NULL,
                        0.f, G_MAXFLOAT, 0.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_HIGHLIGHT] =
    g_param_spec_float ("highlight", NULL, NULL,
                        0.f, 1.f, 0.35f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_GLOW_WEIGHT] =
    g_param_spec_float ("glow-weight", NULL, NULL,
                        0.f, 100.f, 15.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_GLOW_BIAS] =
    g_param_spec_float ("glow-bias", NULL, NULL,
                        -100.f, 100.f, 0.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_GLOW_BEVEL] =
    g_param_spec_float ("glow-bevel", NULL, NULL,
                        0.f, 100.f, 3.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_GLOW_SMOOTH] =
    g_param_spec_float ("glow-smooth", NULL, NULL,
                        0.f, 100.f, 10.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_REFRACTION] =
    g_param_spec_float ("refraction", NULL, NULL,
                        0.f, 150.f, 20.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_DEPTH] =
    g_param_spec_float ("depth", NULL, NULL,
                        0.f, 150.f, 70.f,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_DEBUG_LOGGING] =
    g_param_spec_boolean ("debug-logging", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gb_liquid_glass_effect_init (GbLiquidGlassEffect *self)
{
  self->mode = GB_BLUR_MODE_ACTOR;
  self->radius = 0;
  self->brightness = 1.f;
  self->adaptive_brightness = FALSE;
  self->adaptive_brightness_strength = 0.75f;
  self->adaptive_brightness_minimum = 0.28f;
  self->adaptive_brightness_quality = GB_ADAPTIVE_BRIGHTNESS_QUALITY_BALANCED;
  self->corner_radius = 0.f;
  self->glow_weight = 15.f;
  self->glow_bias = 0.f;
  self->glow_bevel = 3.f;
  self->glow_smooth = 10.f;
  self->refraction = 20.f;
  self->depth = 70.f;

  self->actor_fb.pipeline = create_base_pipeline ();
  self->background_fb.pipeline = create_base_pipeline ();
  rebuild_brightness_pipeline (self);
  self->mask_fb.pipeline = create_mask_pipeline ();
  self->corner_radius_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline,
                                        "u_corner_radius") : -1;
  self->mask_size_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline, "u_size") : -1;
  self->glow_weight_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline, "u_glow_weight") : -1;
  self->glow_bias_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline, "u_glow_bias") : -1;
  self->glow_bevel_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline, "u_glow_bevel") : -1;
  self->glow_smooth_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline, "u_glow_smooth") : -1;
  self->refraction_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline, "u_refraction") : -1;
  self->depth_uniform = self->mask_fb.pipeline ?
    cogl_pipeline_get_uniform_location (self->mask_fb.pipeline, "u_depth") : -1;
}

GbLiquidGlassEffect *
gb_liquid_glass_effect_new (void)
{
  return g_object_new (GB_TYPE_LIQUID_GLASS_EFFECT, NULL);
}

int
gb_liquid_glass_effect_get_radius (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0);

  return self->radius;
}

void
gb_liquid_glass_effect_set_radius (GbLiquidGlassEffect *self,
                                   int                  radius)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  radius = CLAMP (radius, 0, MAX_EFFECT_RADIUS);

  if (self->radius == radius)
    return;

  self->radius = radius;
  self->cache_flags &= ~BLUR_APPLIED;

  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] radius=%d queue-repaint=%d actor=%p",
                 (void *) self, self->radius, self->actor != NULL, (void *) self->actor);

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RADIUS]);
}

float
gb_liquid_glass_effect_get_brightness (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), -1);

  return self->brightness;
}

void
gb_liquid_glass_effect_set_brightness (GbLiquidGlassEffect *self,
                                       float                brightness)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  brightness = sanitize_float_property (brightness, 0.f, 1.f, 1.f);

  if (self->brightness == brightness)
    return;

  self->brightness = brightness;
  self->cache_flags &= ~BLUR_APPLIED;

  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] brightness=%.3f queue-repaint=%d actor=%p",
                 (void *) self, self->brightness, self->actor != NULL, (void *) self->actor);

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BRIGHTNESS]);
}

gboolean
gb_liquid_glass_effect_get_adaptive_brightness (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), FALSE);

  return self->adaptive_brightness;
}

void
gb_liquid_glass_effect_set_adaptive_brightness (GbLiquidGlassEffect *self,
                                                gboolean             adaptive_brightness)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  adaptive_brightness = !!adaptive_brightness;

  if (self->adaptive_brightness == adaptive_brightness)
    return;

  self->adaptive_brightness = adaptive_brightness;
  self->cache_flags &= ~BLUR_APPLIED;

  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] adaptive-brightness=%d queue-repaint=%d actor=%p",
                 (void *) self, self->adaptive_brightness, self->actor != NULL, (void *) self->actor);

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ADAPTIVE_BRIGHTNESS]);
}

float
gb_liquid_glass_effect_get_adaptive_brightness_strength (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);

  return self->adaptive_brightness_strength;
}

void
gb_liquid_glass_effect_set_adaptive_brightness_strength (GbLiquidGlassEffect *self,
                                                         float                strength)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  strength = sanitize_float_property (strength, 0.f, 1.f, 0.75f);

  if (self->adaptive_brightness_strength == strength)
    return;

  self->adaptive_brightness_strength = strength;
  self->cache_flags &= ~BLUR_APPLIED;

  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] adaptive-brightness-strength=%.3f queue-repaint=%d actor=%p",
                 (void *) self, self->adaptive_brightness_strength, self->actor != NULL, (void *) self->actor);

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ADAPTIVE_BRIGHTNESS_STRENGTH]);
}

float
gb_liquid_glass_effect_get_adaptive_brightness_minimum (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);

  return self->adaptive_brightness_minimum;
}

void
gb_liquid_glass_effect_set_adaptive_brightness_minimum (GbLiquidGlassEffect *self,
                                                        float                minimum)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  minimum = sanitize_float_property (minimum, 0.f, 1.f, 0.28f);

  if (self->adaptive_brightness_minimum == minimum)
    return;

  self->adaptive_brightness_minimum = minimum;
  self->cache_flags &= ~BLUR_APPLIED;

  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] adaptive-brightness-minimum=%.3f queue-repaint=%d actor=%p",
                 (void *) self, self->adaptive_brightness_minimum, self->actor != NULL, (void *) self->actor);

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ADAPTIVE_BRIGHTNESS_MINIMUM]);
}

GbAdaptiveBrightnessQuality
gb_liquid_glass_effect_get_adaptive_brightness_quality (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self),
                        GB_ADAPTIVE_BRIGHTNESS_QUALITY_BALANCED);

  return self->adaptive_brightness_quality;
}

void
gb_liquid_glass_effect_set_adaptive_brightness_quality (GbLiquidGlassEffect         *self,
                                                        GbAdaptiveBrightnessQuality  quality)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  if (quality < GB_ADAPTIVE_BRIGHTNESS_QUALITY_PERFORMANCE ||
      quality > GB_ADAPTIVE_BRIGHTNESS_QUALITY_QUALITY)
    quality = GB_ADAPTIVE_BRIGHTNESS_QUALITY_BALANCED;

  if (self->adaptive_brightness_quality == quality)
    return;

  self->adaptive_brightness_quality = quality;

  GB_BLUR_DEBUG ("LiquidGlassEffect[%p] adaptive-brightness-quality=%d queue-repaint=%d actor=%p",
                 (void *) self, self->adaptive_brightness_quality, self->actor != NULL, (void *) self->actor);

  /* Swap in the pre-compiled pipeline for this quality level. */
  clear_framebuffer_data (&self->brightness_fb);
  rebuild_brightness_pipeline (self);

  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ADAPTIVE_BRIGHTNESS_QUALITY]);
}

GbBlurMode
gb_liquid_glass_effect_get_mode (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), GB_BLUR_MODE_ACTOR);

  return self->mode;
}

void
gb_liquid_glass_effect_set_mode (GbLiquidGlassEffect *self,
                                 GbBlurMode           mode)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  if (!is_valid_mode (mode))
    mode = GB_BLUR_MODE_ACTOR;

  if (self->mode == mode)
    return;

  self->mode = mode;
  self->cache_flags &= ~BLUR_APPLIED;

  switch (mode)
    {
    case GB_BLUR_MODE_ACTOR:
      clear_framebuffer_data (&self->background_fb);
      break;

    case GB_BLUR_MODE_BACKGROUND:
      clear_framebuffer_data (&self->actor_fb);
      self->cache_flags &= ~ACTOR_PAINTED;
      break;

    default:
      break;
    }

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODE]);
}

float
gb_liquid_glass_effect_get_corner_radius (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->corner_radius;
}

void
gb_liquid_glass_effect_set_corner_radius (GbLiquidGlassEffect *self,
                                          float                corner_radius)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  corner_radius = sanitize_float_property (corner_radius, 0.f, G_MAXFLOAT, 0.f);

  if (self->corner_radius == corner_radius)
    return;

  self->corner_radius = corner_radius;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CORNER_RADIUS]);
}

float
gb_liquid_glass_effect_get_highlight (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->glow_weight / 42.85714f;
}

void
gb_liquid_glass_effect_set_highlight (GbLiquidGlassEffect *self,
                                      float                highlight)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  highlight = sanitize_float_property (highlight, 0.f, 1.f, 0.f);
  gb_liquid_glass_effect_set_glow_weight (self, highlight * 42.85714f);
}

float
gb_liquid_glass_effect_get_glow_weight (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->glow_weight;
}

void
gb_liquid_glass_effect_set_glow_weight (GbLiquidGlassEffect *self,
                                        float                glow_weight)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  glow_weight = sanitize_float_property (glow_weight, 0.f, 100.f, 15.f);

  if (self->glow_weight == glow_weight)
    return;

  self->glow_weight = glow_weight;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLOW_WEIGHT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HIGHLIGHT]);
}

float
gb_liquid_glass_effect_get_glow_bias (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->glow_bias;
}

void
gb_liquid_glass_effect_set_glow_bias (GbLiquidGlassEffect *self,
                                      float                glow_bias)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  glow_bias = sanitize_float_property (glow_bias, -100.f, 100.f, 0.f);

  if (self->glow_bias == glow_bias)
    return;

  self->glow_bias = glow_bias;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLOW_BIAS]);
}

float
gb_liquid_glass_effect_get_glow_bevel (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->glow_bevel;
}

void
gb_liquid_glass_effect_set_glow_bevel (GbLiquidGlassEffect *self,
                                       float                glow_bevel)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  glow_bevel = sanitize_float_property (glow_bevel, 0.f, 100.f, 3.f);

  if (self->glow_bevel == glow_bevel)
    return;

  self->glow_bevel = glow_bevel;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLOW_BEVEL]);
}

float
gb_liquid_glass_effect_get_glow_smooth (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->glow_smooth;
}

void
gb_liquid_glass_effect_set_glow_smooth (GbLiquidGlassEffect *self,
                                        float                glow_smooth)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  glow_smooth = sanitize_float_property (glow_smooth, 0.f, 100.f, 10.f);

  if (self->glow_smooth == glow_smooth)
    return;

  self->glow_smooth = glow_smooth;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLOW_SMOOTH]);
}

float
gb_liquid_glass_effect_get_refraction (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->refraction;
}

void
gb_liquid_glass_effect_set_refraction (GbLiquidGlassEffect *self,
                                       float                refraction)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  refraction = sanitize_float_property (refraction, 0.f, 150.f, 20.f);

  if (self->refraction == refraction)
    return;

  self->refraction = refraction;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REFRACTION]);
}

float
gb_liquid_glass_effect_get_depth (GbLiquidGlassEffect *self)
{
  g_return_val_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self), 0.f);
  return self->depth;
}

void
gb_liquid_glass_effect_set_depth (GbLiquidGlassEffect *self,
                                  float                depth)
{
  g_return_if_fail (GB_IS_LIQUID_GLASS_EFFECT (self));

  depth = sanitize_float_property (depth, 0.f, 150.f, 70.f);

  if (self->depth == depth)
    return;

  self->depth = depth;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEPTH]);
}
