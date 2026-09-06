#include "openwow/render/m2/m2_model_preparation.h"

#include "openwow/render/m2/m2_model_repository.h"
#include "openwow/render/m2/m2_skin_profile.h"
#include "openwow/render/m2/m2_texture_unit_preparation.h"
#include "openwow/foundation/diagnostics/logging.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace openwow::render::m2 {

namespace {

[[nodiscard]] bool M2ModelDiagEnabled() {
  static const bool enabled = [] {
    const char *value = std::getenv("OPENWOW_M2_DIAG");
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

void TraceParsedModelAnimationTable(const std::string &path,
                                    const data::model::M2Model &model) {
  if (!M2ModelDiagEnabled()) {
    return;
  }
  std::string first_ids;
  std::string first_flags;
  std::size_t external_count = 0u;
  const std::size_t shown =
      std::min<std::size_t>(model.animation_sequences.size(), 12u);
  for (std::size_t i = 0; i < shown; ++i) {
    if (i != 0u) {
      first_ids += ",";
      first_flags += ",";
    }
    first_ids += std::to_string(model.animation_sequences[i].animation_id);
    first_flags += std::to_string(model.animation_sequences[i].flags);
  }
  for (const auto &sequence : model.animation_sequences) {
    if (data::model::M2SequenceUsesExternalData(sequence)) {
      ++external_count;
    }
  }
  openwow::diagnostics::Log(
      openwow::diagnostics::LogLevel::kInfo,
      "M2Diag: model path=" + path +
          " version=" + std::to_string(model.header.version) +
          " sequences=" +
          std::to_string(model.animation_sequences.size()) +
          " external=" + std::to_string(external_count) +
          " playable=" +
          std::to_string(model.playable_animation_lookup.size()) +
          " bones=" + std::to_string(model.bones.size()) +
          " first_ids=[" + first_ids + "]" + " first_flags=[" +
          first_flags + "]");
}

}

M2PreparedModel::M2PreparedModel() = default;
M2PreparedModel::~M2PreparedModel() = default;
M2PreparedModel::M2PreparedModel(M2PreparedModel&&) noexcept = default;
M2PreparedModel& M2PreparedModel::operator=(M2PreparedModel&&) noexcept = default;

const std::vector<M2ModelTextureDependency>&
M2PreparedModel::TextureDependencies() const noexcept {
  static const std::vector<M2ModelTextureDependency> empty;
  return impl_ ? impl_->texture_dependencies : empty;
}

const M2ModelSpatialInfo& M2PreparedModel::SpatialInfo() const noexcept {
  static const M2ModelSpatialInfo empty;
  return impl_ ? impl_->spatial_info : empty;
}

std::shared_ptr<const M2ModelCollisionGeometry>
M2PreparedModel::CollisionGeometry() const noexcept {
  return impl_ ? impl_->resource.collision_geometry : nullptr;
}

bool M2PreparedModel::Valid() const noexcept {
  return impl_ != nullptr && impl_->resource.loaded;
}

std::unique_ptr<M2PreparedModel> M2PreparedModelAccess::Create(
    detail::M2ModelResource resource, std::string cache_key,
    std::vector<M2ModelTextureDependency> texture_dependencies,
    M2ModelSpatialInfo spatial_info) {
  auto prepared = std::make_unique<M2PreparedModel>();
  prepared->impl_ = std::make_unique<M2PreparedModel::Impl>();
  prepared->impl_->resource = std::move(resource);
  prepared->impl_->cache_key = std::move(cache_key);
  prepared->impl_->texture_dependencies = std::move(texture_dependencies);
  prepared->impl_->spatial_info = spatial_info;
  return prepared;
}

M2PreparedModel::Impl* M2PreparedModelAccess::Get(
    M2PreparedModel* prepared) noexcept {
  return prepared != nullptr ? prepared->impl_.get() : nullptr;
}

namespace {

std::shared_ptr<const M2ModelCollisionGeometry> BuildCollisionGeometry(
    const data::model::M2Model& model) {
  auto geometry = std::make_shared<M2ModelCollisionGeometry>();
  geometry->vertices.reserve(model.bounding_vertices.size());
  float radius = model.header.collision_sphere_radius;
  for (const auto& vertex : model.bounding_vertices) {
    geometry->vertices.push_back({vertex.x, vertex.y, vertex.z});
    radius = std::max(radius, std::sqrt(vertex.x * vertex.x +
                                        vertex.y * vertex.y +
                                        vertex.z * vertex.z));
  }
  geometry->triangles = model.bounding_triangles;
  geometry->radius = radius;
  return geometry;
}

M2ModelSpatialInfo BuildM2ModelSpatialInfoFromBoundingBox(
    const float (&bounding_box_min)[3], const float (&bounding_box_max)[3],
    const float bounding_sphere_radius) {
  M2ModelSpatialInfo info;
  info.local_bounds = {bounding_box_min[0], bounding_box_min[1],
                       bounding_box_min[2], bounding_box_max[0],
                       bounding_box_max[1], bounding_box_max[2]};
  info.local_bounding_sphere = {
      (bounding_box_min[0] + bounding_box_max[0]) * 0.5f,
      (bounding_box_min[1] + bounding_box_max[1]) * 0.5f,
      (bounding_box_min[2] + bounding_box_max[2]) * 0.5f,
      bounding_sphere_radius};
  return info;
}

}

M2ModelSpatialInfo BuildM2ModelSpatialInfo(const detail::M2ModelResource& resource) {
  const auto& header = resource.model_data.header;
  return BuildM2ModelSpatialInfoFromBoundingBox(
      header.bounding_box_min, header.bounding_box_max, header.bounding_sphere_radius);
}

M2ModelSpatialInfo BuildM2ModelSpatialInfoFromHeaderBounds(
    const data::model::M2HeaderBoundsResult& bounds) {
  return BuildM2ModelSpatialInfoFromBoundingBox(
      bounds.bounding_box_min, bounds.bounding_box_max, bounds.bounding_sphere_radius);
}

M2ModelPrepareResult PrepareM2ModelPackage(
    const std::string& path, const bool require_render_data,
    const std::uint32_t skin_profile_quality,
    const std::function<std::vector<std::uint8_t>(const std::string&)>& loader,
    const M2CpuModelBuilder& build_cpu,
    const M2ModelEarlyReadyCallback& on_early_ready) {
  if (path.empty()) {
    return {.status = M2ResultStatus::kFailed,
            .reason = M2ResultReason::kInvalidHandle,
            .detail = "empty model path"};
  }
  if (!loader) {
    return {.status = M2ResultStatus::kNotReady,
            .reason = M2ResultReason::kMissingFile,
            .detail = "no M2 file loader"};
  }
  const M2ModelIdentity identity = M2ModelRepository::Canonicalize(path);
  if (!identity.valid) {
    return {.status = M2ResultStatus::kFailed,
            .reason = M2ResultReason::kParseFailed,
            .detail = "invalid model extension: " + path};
  }

  auto bytes = loader(identity.load_path);
  if (bytes.empty()) {
    return {.status = M2ResultStatus::kFailed,
            .reason = M2ResultReason::kMissingFile,
            .detail = identity.load_path};
  }
  auto parsed = data::model::LoadM2FromBytes(bytes);
  if (!parsed.ok) {
    return {.status = M2ResultStatus::kFailed,
            .reason = M2ResultReason::kParseFailed,
            .detail = parsed.error};
  }
  TraceParsedModelAnimationTable(identity.load_path, parsed.model);

  detail::M2ModelResource resource;
  resource.model_path = identity.load_path;
  resource.model_data = std::move(parsed.model);
  resource.collision_geometry = BuildCollisionGeometry(resource.model_data);
  resource.source_model_bytes =
      std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
  resource.loaded = true;

  if (on_early_ready) {
    on_early_ready(BuildM2ModelSpatialInfo(resource), resource.collision_geometry);
  }

  if (require_render_data) {
    const auto profile =
        SelectM2SkinProfile(resource.model_data, skin_profile_quality);
    if (!profile) {
      return {.status = M2ResultStatus::kFailed,
              .reason = M2ResultReason::kSkinProfileUnavailable,
              .detail = identity.load_path};
    }
    data::model::SkinLoadResult skin;
    std::string skin_path;
    if (resource.model_data.header.version == 256u) {
      // Classic v256 keeps its view/profile records in the M2 itself.  The
      // later .skin sidecar convention must not be applied to these files.
      skin = data::model::LoadClassicEmbeddedSkinFromM2Bytes(
          *resource.source_model_bytes, *profile);
      skin_path = identity.load_path + "#embedded-skin";
    } else {
      skin_path = BuildM2SkinProfilePath(identity.load_path, *profile);
      const auto skin_bytes = loader(skin_path);
      if (skin_bytes.empty()) {
        return {.status = M2ResultStatus::kFailed,
                .reason = M2ResultReason::kMissingFile,
                .detail = skin_path};
      }
      skin = data::model::LoadSkinFromBytes(skin_bytes);
    }
    if (!skin.ok) {
      return {.status = M2ResultStatus::kFailed,
              .reason = M2ResultReason::kParseFailed,
              .detail = skin_path + ": " + skin.error};
    }

    const M2ResourcePreparationResult built =
        build_cpu(resource.model_data, skin.skin, resource);
    if (built.status != M2ResultStatus::kReady) {
      return {.status = built.status,
              .reason = built.reason,
              .detail = built.detail};
    }
    resource.selected_skin_profile = *profile;
  } else {
    resource.bounds_min = {resource.model_data.header.bounding_box_min[0],
                           resource.model_data.header.bounding_box_min[1],
                           resource.model_data.header.bounding_box_min[2]};
    resource.bounds_max = {resource.model_data.header.bounding_box_max[0],
                           resource.model_data.header.bounding_box_max[1],
                           resource.model_data.header.bounding_box_max[2]};
    resource.bounds_radius = resource.model_data.header.bounding_sphere_radius;
    resource.has_bones = !resource.model_data.bones.empty();
    resource.has_billboard_bones =
        openwow::render::m2::detail::ComputeM2ModelHasBillboardBones(resource.model_data);
    resource.has_live_global_sequence =
        openwow::render::m2::detail::ComputeM2ModelHasLiveGlobalSequence(resource.model_data);
  }

  std::vector<M2ModelTextureDependency> textures;
  std::vector<std::uint16_t> seen_texture_indices;
  const auto add_texture_index = [&](const std::uint16_t index) {
    if (index >= resource.model_data.textures.size() ||
        std::find(seen_texture_indices.begin(), seen_texture_indices.end(), index) !=
            seen_texture_indices.end()) {
      return;
    }
    const auto& texture = resource.model_data.textures[index];
    if (texture.type != 0u || texture.name_text.empty()) {
      return;
    }
    seen_texture_indices.push_back(index);
    textures.push_back({.texture_index = index, .texture_path = texture.name_text});
  };

  if (!resource.skin_data.texture_units.empty()) {
    // The render-batch cache is populated after this package is prepared.
    // Classic models already expose their authoritative texture combos in the
    // embedded skin data, so use those combos even during the initial pass.
    // Falling back to every raw texture record here would make an unused
    // malformed record (for example "inter") abort the whole model.
    for (const auto& unit : resource.skin_data.texture_units) {
      const auto shader = ResolveM2SkinTextureUnitShader(resource.model_data, unit);
      if (!shader.valid || !shader.draws || shader.texture_count == 0u) continue;
      const auto combos = ResolveM2SkinTextureUnitCombos(resource.model_data, unit);
      if (!combos.primary_texture_valid) continue;
      add_texture_index(combos.primary_texture_index);
      if (shader.texture_count > 1u && combos.secondary_texture_index.has_value()) {
        add_texture_index(*combos.secondary_texture_index);
      }
    }
  } else if (!resource.IsReadyForRender()) {
    for (std::size_t index = 0; index < resource.model_data.textures.size(); ++index) {
      if (index > std::numeric_limits<std::uint16_t>::max()) break;
      add_texture_index(static_cast<std::uint16_t>(index));
    }
  } else {
    // Benilla/Vanilla resolves M2 texture filenames from the texture-combo
    // records used by render batches. Do not make an unused or malformed
    // filename record fail an otherwise drawable model.
    for (const auto& unit : resource.skin_data.texture_units) {
      const auto shader = ResolveM2SkinTextureUnitShader(resource.model_data, unit);
      if (!shader.valid || !shader.draws || shader.texture_count == 0u) continue;
      const auto combos = ResolveM2SkinTextureUnitCombos(resource.model_data, unit);
      if (!combos.primary_texture_valid) continue;
      add_texture_index(combos.primary_texture_index);
      if (shader.texture_count > 1u && combos.secondary_texture_index.has_value()) {
        add_texture_index(*combos.secondary_texture_index);
      }
    }
    for (const auto& emitter : resource.model_data.particle_emitters) {
      add_texture_index(emitter.texture);
    }
    for (const auto& ribbon : resource.model_data.ribbon_emitters) {
      for (const auto index : ribbon.texture_indices) add_texture_index(index);
    }
  }
  const M2ModelSpatialInfo spatial_info = BuildM2ModelSpatialInfo(resource);
  auto prepared = M2PreparedModelAccess::Create(
      std::move(resource), identity.cache_key, std::move(textures), spatial_info);
  return {.status = M2ResultStatus::kReady, .prepared = std::move(prepared)};
}

}
