#include "openwow/render/scene/shadow_presentation_runtime.h"
#include "openwow/foundation/diagnostics/logging.h"

#include "openwow/render/world/doodads/doodad_renderer.h"
#include "openwow/render/m2/m2_shadow_receiver.h"
#include "openwow/render/m2/m2_system.h"
#include "openwow/render/world/terrain/terrain_renderer.h"
#include "openwow/world/coordinates/frustum.h"
#include "openwow/world/presentation/world_presentation_snapshot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace openwow::render {
namespace {

constexpr double kShadowCasterRenderMicroseconds = 1.15;

ShadowQuality ResolveQuality(const std::uint8_t quality) {
  return static_cast<ShadowQuality>(
      std::min<std::uint8_t>(quality,
                             static_cast<std::uint8_t>(ShadowQuality::Ultra)));
}

constexpr std::uint64_t kFnv1aOffsetBasis = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnv1aPrime = 0x100000001b3ull;

[[nodiscard]] std::uint64_t HashBytes(std::uint64_t hash, const void *const data,
                                      const std::size_t size) {
  const auto *const bytes = static_cast<const std::uint8_t *>(data);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= kFnv1aPrime;
  }
  return hash;
}

template <typename T>
[[nodiscard]] std::uint64_t HashValue(const std::uint64_t hash, const T &value) {
  static_assert(std::is_trivially_copyable_v<T>);
  return HashBytes(hash, &value, sizeof(T));
}

[[nodiscard]] std::uint64_t HashMatrix(const std::uint64_t hash, const float *const matrix) {
  return HashBytes(hash, matrix, sizeof(float) * 16u);
}

constexpr std::int8_t kStaticInstancingClassNoResidue = 1;
constexpr std::int8_t kStaticInstancingClassTransparentResidue = 2;

}

ShadowPresentationRuntime::ShadowPresentationRuntime(m2::M2System& m2_system)
    : m2_system_(m2_system),
      data_(std::make_unique<ShadowRenderData>()),
      far_data_(std::make_unique<ShadowRenderData>()) {}

ShadowPresentationRuntime::~ShadowPresentationRuntime() { Shutdown(); }

bool ShadowPresentationRuntime::Initialize() {
  if (initialized_) {
    return true;
  }
  data_->SetType(ShadowType::ShadowMap);
  data_->SetEnabled(true);

  InvalidateShadowReuse();
  initialized_ = data_->CreateShadowMap();
  if (initialized_) {
    resolution_ =
        static_cast<std::uint16_t>(data_->GetShadowMapResolution());
  }

  return true;
}

void ShadowPresentationRuntime::Shutdown() {
  if (!data_) {
    return;
  }
  data_->DestroyShadowMap();
  data_->ClearCasters();
  far_data_->DestroyShadowMap();
  far_data_->ClearCasters();
  far_initialized_ = false;
  far_instance_ids_.clear();
  far_casters_.clear();
  casters_.clear();
  instance_ids_.clear();
  InvalidateShadowReuse();
  resolution_ = 0;
  initialized_ = false;
}

void ShadowPresentationRuntime::ResetMap() {
  if (data_) {
    data_->ClearCasters();
    casters_.clear();
    instance_ids_.clear();
    InvalidateShadowReuse();
  }
}

void ShadowPresentationRuntime::InvalidateShadowReuse() noexcept {
  has_rendered_key_ = false;
  has_previous_content_hash_ = false;
  has_far_rendered_hash_ = false;
}

void ShadowPresentationRuntime::ApplySettings(const world::WorldPresentationSnapshot &snapshot) {
  const auto &settings = snapshot.shadows;
  // Dag/nacht: de zonhoogte (z van de richting naar de zon) stuurt de sterkte. Onder ~7 graden
  // (nacht, schemering) geen schaduw: de kaart zou dan van onderaf of bijna vlak projecteren,
  // en de schaduwen zijn dan veel langer dan de kaart. Vol vanaf ~24 graden. Bij sterkte 0
  // slaan we de hele schaduwpass over.
  const float sun_height = snapshot.environment.light_direction[2];
  const float day_t = std::clamp((sun_height - 0.12f) / 0.28f, 0.0f, 1.0f);
  const float day = day_t * day_t * (3.0f - 2.0f * day_t);
  const float strength = std::clamp(settings.strength, 0.0f, 1.0f) * day;
  data_->SetStrength(strength);
  data_->SetEnabled(settings.enabled && strength > 0.01f);
  data_->SetQuality(ResolveQuality(settings.quality));
  data_->SetShadowDistance(std::max(settings.distance, 1.0f));
  data_->SetShadowBias(std::max(settings.depth_bias, 0.0f));
  data_->SetLightDirection(snapshot.environment.light_direction[0],
                           snapshot.environment.light_direction[1],
                           snapshot.environment.light_direction[2]);

  if (settings.map_resolution != resolution_) {
    data_->DestroyShadowMap();
    data_->SetShadowMapResolution(settings.map_resolution);
    resolution_ = static_cast<std::uint16_t>(data_->GetShadowMapResolution());
    initialized_ = false;

    InvalidateShadowReuse();
  }
  if (settings.enabled && !initialized_) {
    InvalidateShadowReuse();
    initialized_ = data_->CreateShadowMap();
  }

  // Verre cascade: een tweede kaart (2048) over `far_distance` yd halve breedte, alleen als de
  // gebruiker een bereik voorbij de near-kaart vraagt. De richting wordt vastgehouden tot de zon
  // meer dan ~0,15 graden verschuift: een kaart die elk frame meedraait zou elk frame opnieuw
  // getekend moeten worden.
  const float near_radius = ShadowRenderData::RadiusForDistance(settings.distance);
  const bool far_wanted = settings.enabled && strength > 0.01f &&
                          settings.far_distance >= near_radius * 1.3f;
  far_data_->SetStrength(strength);
  if (far_wanted && !far_initialized_) {
    far_data_->SetType(ShadowType::ShadowMap);
    far_data_->SetEnabled(true);
    far_data_->SetShadowMapResolution(2048);
    far_initialized_ = far_data_->CreateShadowMap();
    has_far_rendered_hash_ = false;
  }
  if (far_wanted && far_initialized_) {
    const float lx = snapshot.environment.light_direction[0];
    const float ly = snapshot.environment.light_direction[1];
    const float lz = snapshot.environment.light_direction[2];
    const float length = std::sqrt(lx * lx + ly * ly + lz * lz);
    if (length > 1e-6f) {
      const float nx = lx / length;
      const float ny = ly / length;
      const float nz = lz / length;
      const float held = far_light_[0] * nx + far_light_[1] * ny + far_light_[2] * nz;
      if (!has_far_light_ || held < 0.999999f) {
        far_light_[0] = nx;
        far_light_[1] = ny;
        far_light_[2] = nz;
        has_far_light_ = true;
      }
    }
    far_data_->SetLightDirection(far_light_[0], far_light_[1], far_light_[2]);
    far_data_->SetRadiusOverride(std::min(settings.far_distance, 500.0f));
    far_data_->SetBiasScale(4.0f);
    far_data_->SetCenterGrid(64.0f, 56.0f);
    far_data_->SetShadowBias(std::max(settings.depth_bias, 0.0f));
  }
  if (!far_wanted && far_initialized_) {
    far_data_->DestroyShadowMap();
    far_initialized_ = false;
    has_far_rendered_hash_ = false;
  }
}

void ShadowPresentationRuntime::Render(const world::WorldPresentationSnapshot &snapshot,
                                       const std::uint8_t shadow_view,
                                       const std::uint8_t far_shadow_view, DoodadRenderer &doodads,
                                       TerrainRenderer &terrain) {
  if (!data_) {
    return;
  }
  ApplySettings(snapshot);
  {
    // Korte statusregel (elke ~300 frames) zodat in een log zichtbaar is of de kaart echt draait.
    static std::uint32_t status_frame = 0u;
    if ((status_frame++ % 300u) == 0u) {
      diagnostics::Log(diagnostics::LogLevel::kInfo,
          "ShadowStatus: enabled=" + std::to_string(snapshot.shadows.enabled) +
          " initialized=" + std::to_string(initialized_) +
          " quality=" + std::to_string(snapshot.shadows.quality) +
          " light=(" + std::to_string(snapshot.environment.light_direction[0]) + "," +
          std::to_string(snapshot.environment.light_direction[1]) + "," +
          std::to_string(snapshot.environment.light_direction[2]) + ")");
    }
  }
  // Niet lezen terwijl de kaart beschreven wordt; aan het eind weer aan.
  m2::ShadowReceiverSlot().store(nullptr, std::memory_order_release);
  if (!initialized_ || !data_->IsEnabled()) {
    InvalidateShadowReuse();
    terrain.SetShadowRenderData(nullptr);
    return;
  }

  const auto &camera = snapshot.camera.position;
  // De kaart en de verzameling hangen aan de speler, niet aan de camera: draaien en zoomen
  // verplaatsen alleen de camera, dus de schaduwen blijven staan.
  const auto &anchor =
      snapshot.camera.has_focus ? snapshot.camera.focus_position : snapshot.camera.position;
  // Alleen wat de stabiele kaart (straal ~80 yd) kan raken, plus marge voor hoge werpers.
  const float gather_radius = ShadowRenderData::RadiusForDistance(snapshot.shadows.distance) * 1.8f;
  const float max_distance_squared = gather_radius * gather_radius;
  constexpr std::size_t kMinInstancedShadowGroupSize = 2u;
  casters_.clear();
  instance_ids_.clear();
  instanced_groups_.clear();
  far_instance_ids_.clear();
  far_casters_.clear();
  const bool far_wanted = far_initialized_ && far_data_->IsEnabled();
  // Verre werpers: grote, statische doodads (grootteklasse >= 2, vanaf ~4 yd) binnen de far-kaart.
  const float far_gather = std::min(snapshot.shadows.far_distance, 500.0f) * 1.5f;
  const float far_gather_squared = far_gather * far_gather;
  std::uint64_t far_hash = kFnv1aOffsetBasis;

  ShadowFrameKey frame_key{};
  frame_key.reusable = true;
  std::uint64_t caster_hash = kFnv1aOffsetBasis;
  // Casters komen uit alle doodads rond de camera, niet alleen uit wat in beeld is: een schaduw
  // hangt aan de wereld en mag niet verschijnen of verdwijnen als je de camera draait.
  doodads.VisitInstancesAroundCamera(camera[0], camera[1], camera[2],
                                [&](const DoodadInstance &instance, const DoodadAdmission &admission) {

                                  if (instance.m2_instance_id == 0u || instance.alpha <= 0.0f) {
                                    return;
                                  }
                                  const float dx = instance.bounding_center[0] - anchor[0];
                                  const float dy = instance.bounding_center[1] - anchor[1];
                                  const float dz = instance.bounding_center[2] - anchor[2];
                                  const float distance_squared = dx * dx + dy * dy + dz * dz;
                                   const bool near_candidate = distance_squared <= max_distance_squared;
                                   const bool far_candidate =
                                       far_wanted && instance.distance_class >= 2u &&
                                       distance_squared <= far_gather_squared;
                                   if (!near_candidate && !far_candidate) {
                                    return;
                                  }
                                  if (instance.shadow_class_memo < 0) {
                                    const auto shadow_class =
                                        m2_system_.QueryShadowClass(instance.m2_instance_id);
                                    if (shadow_class.status != m2::M2ResultStatus::kReady) {
                                      return;
                                    }
                                    instance.shadow_class_memo =
                                        static_cast<std::int32_t>(shadow_class.shadow_class);
                                  }
                                  if (far_candidate) {
                                     far_instance_ids_.push_back(instance.m2_instance_id);
                                     far_casters_.push_back(
                                         ShadowCasterEntry{.entityId = instance.m2_instance_id,
                                                           .isValid = true});
                                     far_hash = HashValue(far_hash, instance.m2_instance_id);
                                     far_hash = HashValue(far_hash, instance.m2_model_id);
                                     far_hash = HashValue(far_hash, instance.model_matrix);
                                   }
                                   if (!near_candidate) {
                                     return;
                                   }
                                   const float radius =
                                      instance.has_bounding_radius
                                          ? std::max(instance.bounding_radius, 0.01f)
                                          : std::max(instance.scale, 0.01f);
                                  caster_hash = HashValue(caster_hash, instance.m2_instance_id);
                                  caster_hash = HashValue(caster_hash, instance.m2_model_id);
                                  caster_hash = HashValue(caster_hash, instance.model_matrix);
                                  caster_hash = HashValue(caster_hash, instance.alpha);
                                  caster_hash = HashValue(caster_hash, instance.tint_color[3]);
                                  caster_hash = HashValue(
                                      caster_hash, admission.distance_alpha);
                                  caster_hash = HashValue(
                                      caster_hash, instance.wmo_color_is_ambient_substitute);
                                  caster_hash =
                                      HashValue(caster_hash, instance.static_instancing_state);
                                  caster_hash =
                                      HashValue(caster_hash, instance.render_ready_latched);

                                  if (!(instance.static_instancing_state ==
                                            kStaticInstancingClassNoResidue ||
                                        (instance.static_instancing_state ==
                                             kStaticInstancingClassTransparentResidue &&
                                         instance.render_ready_latched))) {
                                    frame_key.reusable = false;
                                  }
                                  casters_.push_back(ShadowCasterEntry{
                                      .entityId = instance.m2_instance_id,
                                      .worldX = instance.bounding_center[0],
                                      .worldY = instance.bounding_center[1],
                                      .worldZ = instance.bounding_center[2],
                                      .radius = radius,
                                      .height = radius * 2.0f,
                                      .isValid = true,
                                  });

                                  if (instance.static_instancing_state ==
                                          kStaticInstancingClassNoResidue &&
                                      instance.alpha == 1.0f) {
                                    auto &group = instanced_groups_[instance.m2_model_id];
                                    if (group.records.empty()) {
                                      group.exemplar_instance_id = instance.m2_instance_id;
                                    }
                                    group.records.push_back(
                                        {.transform = instance.model_matrix,
                                         .color = {1.0f, 1.0f, 1.0f, 1.0f}});
                                    group.member_ids.push_back(instance.m2_instance_id);
                                    return;
                                  }
                                  instance_ids_.push_back(instance.m2_instance_id);
                                });

  if (extra_caster_provider_) {
    extra_caster_ids_.clear();
    extra_caster_provider_(anchor[0], anchor[1], anchor[2], gather_radius, extra_caster_ids_);
    for (const std::uint32_t id : extra_caster_ids_) {
      // Units bewegen en animeren: dat frame hergebruiken we niet.
      frame_key.reusable = false;
      caster_hash = HashValue(caster_hash, id);
      casters_.push_back(ShadowCasterEntry{.entityId = id, .isValid = true});
      instance_ids_.push_back(id);
    }
  }

  bool has_instanced_groups = false;
  for (auto &[model_id, group] : instanced_groups_) {
    (void)model_id;
    if (group.records.size() >= kMinInstancedShadowGroupSize) {
      has_instanced_groups = true;
    } else {

      instance_ids_.insert(instance_ids_.end(), group.member_ids.begin(),
                           group.member_ids.end());
      group.records.clear();
      group.member_ids.clear();
    }
  }

  // Verre cascade: bouw hem, en teken alleen opnieuw als zijn inhoud veranderde (de werpers zijn
  // statisch; het licht staat vast tot de zon merkbaar verschuift).
  bool far_ready = false;
  if (far_wanted && !far_instance_ids_.empty()) {
    far_data_->SetCasters(far_casters_);
    far_data_->SetCameraAnchor(anchor.data(), snapshot.camera.forward.data());
    if (far_data_->PrepareShadowPass(snapshot.camera.view.data(), snapshot.camera.projection.data(),
                                     snapshot.camera.near_clip, snapshot.camera.far_clip)) {
      std::uint64_t far_content = far_hash;
      far_content = HashValue(far_content, snapshot.map_generation.value);
      far_content = HashMatrix(far_content, far_data_->GetLightView());
      far_content = HashMatrix(far_content, far_data_->GetLightProj());
      if (!has_far_rendered_hash_ || far_content != far_rendered_hash_) {
        far_data_->BeginShadowDepthPass(far_shadow_view);
        render_results_scratch_.resize(far_instance_ids_.size());
        m2_system_.RenderInstanceBatch(far_shadow_view, far_instance_ids_,
                                       RenderMatrix4x4View{far_data_->GetLightView(), 16u},
                                       m2::M2RenderPassScope::kOpaqueOnly,
                                       m2_system_.frame_job_system(),
                                       kShadowCasterRenderMicroseconds, render_results_scratch_);
        far_rendered_hash_ = far_content;
        has_far_rendered_hash_ = true;
      }
      far_ready = true;
    }
  }
  data_->SetFarCascade(far_ready ? far_data_.get() : nullptr);

  data_->SetCasters(casters_);
  if (instance_ids_.empty() && !has_instanced_groups && !far_ready) {
    InvalidateShadowReuse();
    terrain.SetShadowRenderData(nullptr);
    return;
  }

  data_->SetCameraAnchor(anchor.data(), snapshot.camera.forward.data());
  if (!data_->PrepareShadowPass(
          snapshot.camera.view.data(), snapshot.camera.projection.data(),
          snapshot.camera.near_clip,
          std::min(snapshot.camera.far_clip, std::max(snapshot.shadows.distance, 1.0f)))) {
    InvalidateShadowReuse();
    terrain.SetShadowRenderData(nullptr);
    return;
  }

  frame_key.caster_count = static_cast<std::uint32_t>(casters_.size());
  std::uint64_t hash = caster_hash;
  hash = HashValue(hash, snapshot.map_generation.value);
  // De kaartinhoud hangt niet van de camera af (alleen van de speler, de casters en het licht):
  // draaien en zoomen mogen het hergebruik niet breken.
  hash = HashValue(hash, anchor);
  hash = HashValue(hash, snapshot.shadows.enabled);
  hash = HashValue(hash, snapshot.shadows.quality);
  hash = HashValue(hash, snapshot.shadows.map_resolution);
  hash = HashValue(hash, snapshot.shadows.distance);
  hash = HashValue(hash, snapshot.shadows.depth_bias);
  hash = HashMatrix(hash, data_->GetLightView());
  hash = HashMatrix(hash, data_->GetLightProj());
  frame_key.content_hash = hash;

  frame_key.previous_content_hash = previous_content_hash_;
  const bool previous_frame_known = has_previous_content_hash_;
  previous_content_hash_ = hash;
  has_previous_content_hash_ = true;

  if (has_rendered_key_ && previous_frame_known && frame_key.reusable &&
      frame_key == rendered_key_) {
    terrain.SetShadowRenderData(data_.get());
    m2::ShadowReceiverSlot().store(data_.get(), std::memory_order_release);
    return;
  }

  data_->BeginShadowDepthPass(shadow_view);

  render_results_scratch_.resize(instance_ids_.size());
  if (!instance_ids_.empty()) {
    m2_system_.RenderInstanceBatch(shadow_view, instance_ids_,
                                   RenderMatrix4x4View{data_->GetLightView(), 16u},
                                   m2::M2RenderPassScope::kOpaqueOnly,
                                   m2_system_.frame_job_system(), kShadowCasterRenderMicroseconds,
                                   render_results_scratch_);
  }

  for (auto &[model_id, group] : instanced_groups_) {
    (void)model_id;
    if (group.records.size() < kMinInstancedShadowGroupSize) {
      continue;
    }
    (void)m2_system_.RenderInstancedGroup(shadow_view, group.exemplar_instance_id,
                                          group.records, m2::M2BatchUniforms{});
  }

  {
    static std::uint32_t caster_log_frame = 0u;
    if ((caster_log_frame++ % 300u) == 0u) {
      diagnostics::Log(diagnostics::LogLevel::kInfo,
          "ShadowStatus: casters=" + std::to_string(instance_ids_.size()) +
          " extra=" + std::to_string(extra_caster_ids_.size()) +
          " far=" + std::to_string(far_instance_ids_.size()) +
          " center=(" + std::to_string(data_->GetShadowCenter()[0]) + "," +
          std::to_string(data_->GetShadowCenter()[1]) + "," +
          std::to_string(data_->GetShadowCenter()[2]) + ") radius=" +
          std::to_string(data_->GetShadowRadius()));
    }
  }
  rendered_key_ = frame_key;
  has_rendered_key_ = previous_frame_known;
  terrain.SetShadowRenderData(data_.get());
  m2::ShadowReceiverSlot().store(data_.get(), std::memory_order_release);
}

}
