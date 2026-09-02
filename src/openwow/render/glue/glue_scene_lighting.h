#pragma once

#include "openwow/render/m2/m2_public_types.h"
#include "openwow/render/m2/m2_public_types.h"
#include "openwow/render/models/animation/model_light_record.h"
#include "openwow/render/models/animation/model_render_callback_pipeline.h"
#include "openwow/render/api/math/render_math_types.h"
#include "openwow/render/world/environment/world_light_accumulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace openwow::render::glue {

struct GlueSceneDirectionalLight {
  RenderVec3 direction{};
  RenderVec3 ambient_rgb{};
  RenderVec3 diffuse_rgb{};
};

struct GlueSceneLightingSnapshot {
  std::vector<GlueSceneDirectionalLight> directional_lights;
  std::vector<ModelRenderCallbackPointLightState> point_lights;

  [[nodiscard]] bool empty() const noexcept {
    return directional_lights.empty() && point_lights.empty();
  }
};

[[nodiscard]] inline RenderVec3 TransformGlueSceneLightPoint(
    const RenderVec3& point, const RenderMatrix4x4View model_world) noexcept {
  return {
      model_world[0] * point[0] + model_world[4] * point[1] +
          model_world[8] * point[2] + model_world[12],
      model_world[1] * point[0] + model_world[5] * point[1] +
          model_world[9] * point[2] + model_world[13],
      model_world[2] * point[0] + model_world[6] * point[1] +
          model_world[10] * point[2] + model_world[14],
  };
}

[[nodiscard]] inline RenderVec3 TransformGlueSceneLightDirection(
    const RenderVec3& direction, const RenderMatrix4x4View model_world) noexcept {
  RenderVec3 transformed{
      model_world[0] * direction[0] + model_world[4] * direction[1] +
          model_world[8] * direction[2],
      model_world[1] * direction[0] + model_world[5] * direction[1] +
          model_world[9] * direction[2],
      model_world[2] * direction[0] + model_world[6] * direction[1] +
          model_world[10] * direction[2],
  };
  const float length_squared = transformed[0] * transformed[0] +
                               transformed[1] * transformed[1] +
                               transformed[2] * transformed[2];
  if (length_squared > ModelLightRecord::kRetailDirectionNormalizeLengthSqEpsilon) {
    const float inverse_length = 1.0f / std::sqrt(length_squared);
    transformed[0] *= inverse_length;
    transformed[1] *= inverse_length;
    transformed[2] *= inverse_length;
  }
  return transformed;
}

inline void AppendGlueSceneM2Light(GlueSceneLightingSnapshot& snapshot,
                                   const m2::M2LightSample& sample,
                                   const RenderMatrix4x4View model_world) {
  if (!sample.visible) {
    return;
  }

  const RenderVec3 diffuse{
      sample.diffuse_color[0] * sample.diffuse_intensity,
      sample.diffuse_color[1] * sample.diffuse_intensity,
      sample.diffuse_color[2] * sample.diffuse_intensity,
  };

  if (sample.type == 1u) {
    ModelRenderCallbackPointLightState point;
    point.position = TransformGlueSceneLightPoint(sample.position, model_world);
    point.diffuse_rgb = diffuse;

    point.attenuation = {
        ModelLightRecord::kRetailSceneLightScalar0,
        ModelLightRecord::kRetailSceneLightScalar1,
        ModelLightRecord::kRetailSceneLightScalar2,
    };
    point.attenuation_model =
        ModelRenderCallbackPointLightAttenuation::kPolynomial;
    snapshot.point_lights.push_back(point);
    return;
  }

  GlueSceneDirectionalLight directional;
  // Vanilla uses the light bone's local +Z axis for a directional M2 light.
  // The M2 position is a point-light input and must not be reused as a
  // directional vector; UI_* scenes commonly contain large position values.
  directional.direction = TransformGlueSceneLightDirection(sample.direction, model_world);
  directional.ambient_rgb = {
      sample.ambient_color[0] * sample.ambient_intensity,
      sample.ambient_color[1] * sample.ambient_intensity,
      sample.ambient_color[2] * sample.ambient_intensity,
  };
  directional.diffuse_rgb = diffuse;
  snapshot.directional_lights.push_back(directional);
}

[[nodiscard]] inline ModelRenderCallbackLightingState ProjectGlueSceneLighting(
    const GlueSceneLightingSnapshot& snapshot) {
  WorldLightAccumulator accumulator;
  accumulator.Initialize(0u, {});

  std::uint32_t light_handle = 1u;
  for (const auto& directional : snapshot.directional_lights) {
    ModelLightRecord record;
    record.Initialize();
    record.enabled = 1u;
    record.light_kind = 0u;
    record.direction = directional.direction;
    record.ambient_rgb = directional.ambient_rgb;
    record.diffuse_rgb = directional.diffuse_rgb;
    accumulator.AccumulateModelLightRecord(light_handle++, record);
  }

  const ProjectedDirectionalLightState projected =
      accumulator.GetProjectedDirectionalState();
  ModelRenderCallbackLightingState state;
  state.has_directional_light = !snapshot.directional_lights.empty();
  state.ambient_rgb = projected.ambient_rgb;
  state.diffuse_rgb = projected.projected_diffuse_rgb;
  state.direction = projected.normalized_light_direction;
  state.directional_lights.reserve(snapshot.directional_lights.size());
  for (const auto& directional : snapshot.directional_lights) {
    state.directional_lights.push_back({
        .direction = directional.direction,
        .ambient_rgb = directional.ambient_rgb,
        .diffuse_rgb = directional.diffuse_rgb,
    });
  }
  state.point_lights = snapshot.point_lights;
  return state;
}

[[nodiscard]] inline bool HasRenderableGlueSceneLighting(
    const ModelRenderCallbackLightingState& state) noexcept {
  const auto has_rgb = [](const RenderVec3& rgb) {
    return rgb[0] != 0.0f || rgb[1] != 0.0f || rgb[2] != 0.0f;
  };
  const bool has_directional_energy =
      state.has_directional_light &&
      (has_rgb(state.ambient_rgb) || has_rgb(state.diffuse_rgb));
  const bool has_preserved_directional_energy = std::any_of(
      state.directional_lights.begin(), state.directional_lights.end(),
      [&has_rgb](const ModelRenderCallbackDirectionalLightState& light) {
        return has_rgb(light.ambient_rgb) || has_rgb(light.diffuse_rgb);
      });
  const bool has_point_energy = std::any_of(
      state.point_lights.begin(), state.point_lights.end(),
      [&has_rgb](const ModelRenderCallbackPointLightState& light) {
        return has_rgb(light.diffuse_rgb);
      });
  return has_directional_energy || has_preserved_directional_energy ||
         has_point_energy;
}

// Classic 1.12 CharacterModelBase constructor defaults, cross-checked against
// Benilla's model_pane_light().  Glue scenes may expose a directional light
// record whose intensities are zero; that record must not turn the character
// preview into an unlit silhouette.
[[nodiscard]] inline ModelRenderCallbackLightingState
BuildClassicCharacterModelLighting() noexcept {
  ModelRenderCallbackLightingState state;
  state.has_directional_light = true;
  state.direction = {0.0f, 1.0f, 0.0f};
  state.ambient_rgb = {0.7f, 0.7f, 0.7f};
  state.diffuse_rgb = {0.8f, 0.8f, 0.64f};
  state.directional_lights.push_back({
      .direction = {0.0f, 1.0f, 0.0f},
      .ambient_rgb = {0.7f, 0.7f, 0.7f},
      .diffuse_rgb = {0.8f, 0.8f, 0.64f},
  });
  return state;
}

[[nodiscard]] inline m2::M2BatchUniforms BuildGlueM2LightingUniforms(
    const ModelRenderCallbackLightingState& state,
    const RenderVec3& model_world_position) {
  m2::M2BatchUniforms uniforms;
  RenderVec3 ambient_rgb{state.ambient_rgb[0], state.ambient_rgb[1],
                         state.ambient_rgb[2]};
  if (!state.directional_lights.empty()) {
    ambient_rgb = {};
    for (const auto& directional : state.directional_lights) {
      ambient_rgb[0] += directional.ambient_rgb[0];
      ambient_rgb[1] += directional.ambient_rgb[1];
      ambient_rgb[2] += directional.ambient_rgb[2];
    }
  }
  uniforms.light_ambient = {
      ambient_rgb[0], ambient_rgb[1], ambient_rgb[2], 0.0f};

  std::size_t uniform_light_count = 0;
  if (!state.directional_lights.empty()) {
    for (const auto& directional : state.directional_lights) {
      if (uniform_light_count >= m2::M2BatchUniforms::kMaxM2Lights) {
        break;
      }
      const std::size_t slot = uniform_light_count++;
      uniforms.light_pos_range[slot] = {
          // Model2.bls consumes a directional lobe as a toward-light vector.
          // M2LightSample::direction is the light bone's local +Z axis, which
          // is already that vector; negating it here lights the back side.
          directional.direction[0], directional.direction[1],
          directional.direction[2], 0.0f};
      uniforms.light_attenuation[slot] = {0.0f, 1.0f, 0.0f, 0.0f};
      uniforms.light_color[slot] = {
          directional.diffuse_rgb[0], directional.diffuse_rgb[1],
          directional.diffuse_rgb[2], 0.0f};
    }
  } else if (state.has_directional_light) {
    const RenderVec3 surface_to_light{state.direction[0], state.direction[1],
                                      state.direction[2]};
    const RenderVec3 non_negative_diffuse{
        std::max(state.diffuse_rgb[0], 0.0f),
        std::max(state.diffuse_rgb[1], 0.0f),
        std::max(state.diffuse_rgb[2], 0.0f)};
    uniforms.light_pos_range[uniform_light_count] = {
        surface_to_light[0], surface_to_light[1], surface_to_light[2], 0.0f};
    uniforms.light_attenuation[uniform_light_count] = {0.0f, 1.0f, 0.0f, 0.0f};
    uniforms.light_color[uniform_light_count] = {
        non_negative_diffuse[0], non_negative_diffuse[1],
        non_negative_diffuse[2], 0.0f};
    ++uniform_light_count;
  }

  const std::size_t point_slot_capacity =
      m2::M2BatchUniforms::kMaxM2Lights - uniform_light_count;
  std::array<const ModelRenderCallbackPointLightState*,
             m2::M2BatchUniforms::kMaxM2Lights>
      selected_points{};
  std::array<float, m2::M2BatchUniforms::kMaxM2Lights>
      selected_distance_squared{};
  selected_distance_squared.fill(std::numeric_limits<float>::infinity());
  std::size_t selected_count = 0;

  for (const auto& point : state.point_lights) {
    if (point_slot_capacity == 0u) {
      break;
    }
    const float dx = point.position[0] - model_world_position[0];
    const float dy = point.position[1] - model_world_position[1];
    const float dz = point.position[2] - model_world_position[2];
    const float distance_squared = dx * dx + dy * dy + dz * dz;

    std::size_t insert_index = selected_count;
    if (insert_index == point_slot_capacity) {
      if (distance_squared >= selected_distance_squared[point_slot_capacity - 1u]) {
        continue;
      }
      insert_index = point_slot_capacity - 1u;
    } else {
      ++selected_count;
    }
    while (insert_index != 0u &&
           distance_squared < selected_distance_squared[insert_index - 1u]) {
      selected_points[insert_index] = selected_points[insert_index - 1u];
      selected_distance_squared[insert_index] =
          selected_distance_squared[insert_index - 1u];
      --insert_index;
    }
    selected_points[insert_index] = &point;
    selected_distance_squared[insert_index] = distance_squared;
  }

  for (std::size_t index = 0; index < selected_count; ++index) {
    const auto& point = *selected_points[index];
    const std::size_t slot = uniform_light_count++;
    uniforms.light_color[slot] = {
        point.diffuse_rgb[0], point.diffuse_rgb[1], point.diffuse_rgb[2], 1.0f};

    if (point.attenuation_model == ModelRenderCallbackPointLightAttenuation::kRange) {
      const float attenuation_end = std::max(point.attenuation[1], 0.01f);
      const float attenuation_start =
          std::clamp(point.attenuation[0], 0.0f, attenuation_end);
      uniforms.light_pos_range[slot] = {
          point.position[0], point.position[1], point.position[2], attenuation_end};
      uniforms.light_attenuation[slot] = {
          attenuation_start, attenuation_end, 0.0f, 1.0f};
    } else {
      uniforms.light_pos_range[slot] = {
          point.position[0], point.position[1], point.position[2], 0.0f};
      uniforms.light_attenuation[slot] = {
          point.attenuation[0], point.attenuation[1], point.attenuation[2], 0.0f};
    }
  }

  uniforms.light_count[0] = static_cast<float>(uniform_light_count);
  return uniforms;
}

}
