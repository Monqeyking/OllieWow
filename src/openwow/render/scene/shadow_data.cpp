
#include "openwow/render/scene/shadow_data.h"

#include "openwow/render/api/draw_encoder.h"
#include "openwow/render/api/math/render_math_types.h"
#include "openwow/foundation/diagnostics/logging.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace openwow::render {

struct ShadowRenderData::BackendResources {
  bgfx::TextureHandle shadow_depth_tex = BGFX_INVALID_HANDLE;
  bgfx::FrameBufferHandle shadow_fbo = BGFX_INVALID_HANDLE;
  bgfx::UniformHandle shadow_map_sampler = BGFX_INVALID_HANDLE;
  bgfx::UniformHandle shadow_matrix = BGFX_INVALID_HANDLE;
  bgfx::UniformHandle shadow_parameters = BGFX_INVALID_HANDLE;
  // De verre cascade (slot 6) wordt door de near-data gebonden met deze eigen uniforms.
  bgfx::UniformHandle far_sampler = BGFX_INVALID_HANDLE;
  bgfx::UniformHandle far_matrix = BGFX_INVALID_HANDLE;
  bgfx::UniformHandle far_parameters = BGFX_INVALID_HANDLE;
};

namespace {

constexpr float kMaxLightUpAlignment = 0.99f;

[[nodiscard]] float SnapOffsetToTexelLattice(const float light_space_offset,
                                             const float world_units_per_texel) {

  if (!(world_units_per_texel > 0.0f) || !std::isfinite(world_units_per_texel)) {
    return 0.0f;
  }
  const float texels = light_space_offset / world_units_per_texel;
  return (std::round(texels) - texels) * world_units_per_texel;
}

void BuildShadowClipToTextureMatrix(float out_matrix[16]) {
  const bgfx::Caps* const caps = bgfx::getCaps();
  const float y_scale = caps->originBottomLeft ? 0.5f : -0.5f;
  const float depth_scale = caps->homogeneousDepth ? 0.5f : 1.0f;
  const float depth_offset = caps->homogeneousDepth ? 0.5f : 0.0f;

  const float matrix[16] = {
      0.5f, 0.0f,    0.0f,        0.0f,
      0.0f, y_scale, 0.0f,        0.0f,
      0.0f, 0.0f,    depth_scale, 0.0f,
      0.5f, 0.5f,    depth_offset, 1.0f,
  };
  std::copy_n(matrix, 16, out_matrix);
}

}

ShadowRenderData::ShadowRenderData()
    : backend_(std::make_unique<BackendResources>()) {}

ShadowRenderData::~ShadowRenderData() { DestroyShadowMap(); }

void ShadowRenderData::SetQuality(ShadowQuality q) { quality_ = q; }
ShadowQuality ShadowRenderData::GetQuality() const { return quality_; }

void ShadowRenderData::SetType(ShadowType t) { type_ = t; }
ShadowType ShadowRenderData::GetType() const { return type_; }

void ShadowRenderData::SetShadowMapResolution(std::uint32_t res) {
    if (res <= 512)       resolution_ = 512;
    else if (res <= 1024) resolution_ = 1024;
    else if (res <= 2048) resolution_ = 2048;
    else                  resolution_ = 4096;
}

std::uint32_t ShadowRenderData::GetShadowMapResolution() const {
    return resolution_;
}

void ShadowRenderData::SetShadowDistance(float dist) {
    distance_ = std::max(0.0f, dist);
}

float ShadowRenderData::GetShadowDistance() const { return distance_; }

void ShadowRenderData::SetShadowBias(float bias) { bias_ = bias; }
float ShadowRenderData::GetShadowBias() const { return bias_; }

void ShadowRenderData::SetSplitLambda(float lambda) {
    split_lambda_ = std::clamp(lambda, 0.0f, 1.0f);
}
float ShadowRenderData::GetSplitLambda() const { return split_lambda_; }

bool ShadowRenderData::CreateShadowMap() {
    if (shadow_map_valid_) return true;
    if (resolution_ == 0) return false;

    DestroyShadowMap();

    const auto width  = static_cast<std::uint16_t>(resolution_);
    const auto height = static_cast<std::uint16_t>(resolution_);

    backend_->shadow_fbo = bgfx::createFrameBuffer(
        width, height,
        bgfx::TextureFormat::D16,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL);

    if (!bgfx::isValid(backend_->shadow_fbo)) {
        openwow::diagnostics::Log(openwow::diagnostics::LogLevel::kWarn,
                           "ShadowRenderData: failed to create shadow map FBO");
        return false;
    }

    backend_->shadow_depth_tex = bgfx::getTexture(backend_->shadow_fbo);

    shadow_map_valid_ = bgfx::isValid(backend_->shadow_depth_tex);
    if (!shadow_map_valid_) {
        DestroyShadowMap();
        return false;
    }

    backend_->shadow_map_sampler =
        bgfx::createUniform("s_shadowMap", bgfx::UniformType::Sampler);
    backend_->shadow_matrix =
        bgfx::createUniform("u_shadowMtx", bgfx::UniformType::Mat4);
    backend_->shadow_parameters =
        bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
    backend_->far_sampler =
        bgfx::createUniform("s_shadowMapFar", bgfx::UniformType::Sampler);
    backend_->far_matrix =
        bgfx::createUniform("u_shadowMtxFar", bgfx::UniformType::Mat4);
    backend_->far_parameters =
        bgfx::createUniform("u_shadowParamsFar", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(backend_->shadow_map_sampler) ||
        !bgfx::isValid(backend_->shadow_matrix) ||
        !bgfx::isValid(backend_->shadow_parameters) ||
        !bgfx::isValid(backend_->far_sampler) ||
        !bgfx::isValid(backend_->far_matrix) ||
        !bgfx::isValid(backend_->far_parameters)) {
        openwow::diagnostics::Log(openwow::diagnostics::LogLevel::kWarn,
                           "ShadowRenderData: failed to create shadow uniforms");
        DestroyShadowMap();
        return false;
    }

    openwow::diagnostics::Log(openwow::diagnostics::LogLevel::kInfo,
                       "ShadowRenderData: shadow map created (" +
                           std::to_string(resolution_) + "x" +
                           std::to_string(resolution_) + ")");
    return true;
}

void ShadowRenderData::DestroyShadowMap() {
    if (bgfx::isValid(backend_->shadow_fbo)) {
        bgfx::destroy(backend_->shadow_fbo);
        backend_->shadow_fbo = BGFX_INVALID_HANDLE;
    }

    backend_->shadow_depth_tex = BGFX_INVALID_HANDLE;

    if (bgfx::isValid(backend_->shadow_map_sampler)) {
        bgfx::destroy(backend_->shadow_map_sampler);
        backend_->shadow_map_sampler = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(backend_->shadow_matrix)) {
        bgfx::destroy(backend_->shadow_matrix);
        backend_->shadow_matrix = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(backend_->shadow_parameters)) {
        bgfx::destroy(backend_->shadow_parameters);
        backend_->shadow_parameters = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(backend_->far_sampler)) {
        bgfx::destroy(backend_->far_sampler);
        backend_->far_sampler = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(backend_->far_matrix)) {
        bgfx::destroy(backend_->far_matrix);
        backend_->far_matrix = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(backend_->far_parameters)) {
        bgfx::destroy(backend_->far_parameters);
        backend_->far_parameters = BGFX_INVALID_HANDLE;
    }

    shadow_map_valid_ = false;
}

bool ShadowRenderData::IsShadowMapValid() const {
    return shadow_map_valid_;
}

void ShadowRenderData::BindShadowState(bgfx::Encoder* const encoder) const {
    if (!shadow_map_valid_ || !bgfx::isValid(backend_->shadow_depth_tex)) return;

    const DrawEncoder draw{encoder};

    draw.setTexture(
        5, backend_->shadow_map_sampler, backend_->shadow_depth_tex);

    draw.setUniform(backend_->shadow_matrix, light_view_proj_);

    const float inv_res = 1.0f / static_cast<float>(resolution_);
    // bias_ is een diepte-bias in kaarteenheden (0,005 bij de oude kaart); hier in wereldeenheden
    // omgerekend, zodat hij niet met de kaartgrootte meeschuift: 0,3 yd plus een texel.
    const float texel_world = (2.0f * radius_) * inv_res;
    const float bias_depth = (0.3f + texel_world) * bias_scale_ / std::max(depth_range_, 1.0f);
    const RenderVec4 params{bias_depth, inv_res, strength_, radius_};
    draw.setUniform(backend_->shadow_parameters, params.data());

    // Verre cascade op slot 6. Zonder (bruikbare) far-kaart: dezelfde textuur nogmaals met sterkte 0,
    // zodat de sampler altijd een geldige textuur heeft en de shader de far-kaart negeert.
    const ShadowRenderData* const far_map =
        far_cascade_ != nullptr && far_cascade_->shadow_map_valid_ &&
                bgfx::isValid(far_cascade_->backend_->shadow_depth_tex) && far_cascade_->strength_ > 0.0f
            ? far_cascade_
            : nullptr;
    if (far_map != nullptr) {
        draw.setTexture(6, backend_->far_sampler, far_map->backend_->shadow_depth_tex);
        draw.setUniform(backend_->far_matrix, far_map->light_view_proj_);
        const float far_inv_res = 1.0f / static_cast<float>(far_map->resolution_);
        const float far_texel_world = (2.0f * far_map->radius_) * far_inv_res;
        const float far_bias =
            (0.3f + far_texel_world) * far_map->bias_scale_ / std::max(far_map->depth_range_, 1.0f);
        const RenderVec4 far_params{far_bias, far_inv_res, far_map->strength_,
                                    static_cast<float>(debug_mode_)};
        draw.setUniform(backend_->far_parameters, far_params.data());
    } else {
        draw.setTexture(6, backend_->far_sampler, backend_->shadow_depth_tex);
        draw.setUniform(backend_->far_matrix, light_view_proj_);
        const RenderVec4 off_params{0.0f, inv_res, 0.0f, static_cast<float>(debug_mode_)};
        draw.setUniform(backend_->far_parameters, off_params.data());
    }
}

void ShadowRenderData::AddCaster(ShadowCasterEntry entry) {
    for (auto& c : casters_) {
        if (c.entityId == entry.entityId) {
            c = entry;
            return;
        }
    }
    casters_.push_back(entry);
}

void ShadowRenderData::RemoveCaster(std::uint32_t entityId) {
  casters_.erase(
      std::remove_if(casters_.begin(), casters_.end(),
                     [entityId](const ShadowCasterEntry &e) { return e.entityId == entityId; }),
      casters_.end());
}

void ShadowRenderData::SetCasters(const std::span<const ShadowCasterEntry> casters) {
  casters_.assign(casters.begin(), casters.end());
}

void ShadowRenderData::ClearCasters() noexcept {
  casters_.clear();
}

std::vector<ShadowCasterEntry> ShadowRenderData::GetCasters() const {
  return casters_;
}

std::uint32_t ShadowRenderData::GetCasterCount() const {
  return static_cast<std::uint32_t>(casters_.size());
}

std::vector<ShadowCasterEntry> ShadowRenderData::GetCastersInRange(float x, float y, float z,
                                                                   float range) const {
  std::vector<ShadowCasterEntry> result;
  const float rangeSq = range * range;
  for (const auto &c : casters_) {
    float dx = c.worldX - x;
    float dy = c.worldY - y;
    float dz = c.worldZ - z;
    if (dx * dx + dy * dy + dz * dz <= rangeSq) {
      result.push_back(c);
    }
  }
  return result;
}

void ShadowRenderData::SetLightDirection(float x, float y, float z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-6f) {
        lightX_ = x / len;
        lightY_ = y / len;
        lightZ_ = z / len;
    } else {
        lightX_ = 0.0f;
        lightY_ = -1.0f;
        lightZ_ = 0.0f;
    }
}

ShadowRenderData::LightDir ShadowRenderData::GetLightDirection() const {
    return {lightX_, lightY_, lightZ_};
}

float ShadowRenderData::RadiusForDistance(const float distance) noexcept {
    // 640 yd (de standaard) -> 80 yd straal; begrensd zodat de resolutie bruikbaar blijft.
    return std::clamp(distance * 0.125f, 30.0f, 120.0f);
}

void ShadowRenderData::BuildLightMatrices(const float* camera_mtx,
                                          const float* proj_mtx,
                                          [[maybe_unused]] float cam_near,
                                          [[maybe_unused]] float cam_far,
                                          float out_light_view[16],
                                          float out_light_proj[16]) {
    // Stabiele kaart: een vaste bol rond het camerastandpunt, niet rond de camerakegel.
    // Het middelpunt hangt alleen af van de oogpositie en de horizontale kijkrichting,
    // nooit van de pitch, en wordt in lichtruimte op het texelraster gesnapt. Daardoor
    // blijft een schaduw op zijn plek in de wereld tijdens lopen, draaien en kijken.
    static_cast<void>(camera_mtx);
    static_cast<void>(proj_mtx);
    const float* const eye = anchor_pos_;
    float forward_x = anchor_fwd_[0];
    float forward_y = anchor_fwd_[1];
    const float forward_length = std::sqrt(forward_x * forward_x + forward_y * forward_y);
    if (forward_length > 1e-4f) {
        forward_x /= forward_length;
        forward_y /= forward_length;
    } else {
        forward_x = 0.0f;
        forward_y = 0.0f;
    }

    radius_ = radius_override_ > 0.0f ? radius_override_ : RadiusForDistance(distance_);
    const float radius = radius_;
    depth_range_ = radius * 4.0f;

    // Het middelpunt ligt op het oog en volgt de kijkrichting niet: de dekking van de kaart is
    // dan onafhankelijk van draaien, en een schaduw blijft op zijn plek in de wereld.
    static_cast<void>(forward_x);
    static_cast<void>(forward_y);
    // Het anker is de speler (zie SetCameraAnchor): draaien en zoomen verplaatsen alleen de
    // camera, dus de kaart blijft staan. Het middelpunt zit bovendien vast aan een wereldrooster
    // van 32 yd en blijft staan tot je meer dan 28 yd ervan af bent (dode zone). Een schaduw
    // hangt aan de wereld: de rand van de kaart en de uitfade liggen zo op vaste plekken, en een
    // boom wordt niet zwakker of korter omdat jij een stap zet.
    const float kCenterGrid = center_grid_;
    const float kCenterHold = center_hold_;
    float center[3];
    for (int axis = 0; axis < 3; ++axis) {
        const bool keep = center_valid_ && std::fabs(eye[axis] - center_[axis]) <= kCenterHold;
        center[axis] = keep ? center_[axis]
                            : std::floor(eye[axis] / kCenterGrid + 0.5f) * kCenterGrid;
    }
    center_valid_ = true;
    center_[0] = center[0];
    center_[1] = center[1];
    center_[2] = center[2];

    const float light_dist = radius * 2.0f;
    float light_pos[3];
    light_pos[0] = center[0] + lightX_ * light_dist;
    light_pos[1] = center[1] + lightY_ * light_dist;
    light_pos[2] = center[2] + lightZ_ * light_dist;

    const float zenith_alignment = std::fabs(lightZ_);
    const bx::Vec3 reference_up = zenith_alignment > kMaxLightUpAlignment
                                      ? bx::Vec3(0.0f, 1.0f, 0.0f)
                                      : bx::Vec3(0.0f, 0.0f, 1.0f);

    bx::mtxLookAt(out_light_view,
                  bx::Vec3(light_pos[0], light_pos[1], light_pos[2]),
                  bx::Vec3(center[0], center[1], center[2]),
                  reference_up,
                  bx::Handedness::Left);

    const float ortho_size = radius;
    // De lichtcamera staat 2R vanaf het middelpunt (naar de zon toe) en kijkt ernaartoe. Het
    // dieptebereik loopt daarom van 0 tot 4R: 2R aan de zonkant van het middelpunt tot 2R erachter,
    // waar de schaduwen vallen. Eerder was het -2R..+2R, waardoor het far-vlak precies door het
    // middelpunt lag en alles aan de schaduwkant daarvan buiten de kaart viel (belicht).
    const float near_p = 0.0f;
    const float far_p  = radius * 4.0f;

    bx::mtxOrtho(out_light_proj,
                 -ortho_size, ortho_size,
                 -ortho_size, ortho_size,
                 near_p, far_p,
                 0.0f,
                 bgfx::getCaps()->homogeneousDepth);

    const float world_units_per_texel =
        (2.0f * ortho_size) / static_cast<float>(resolution_);
    out_light_proj[12] +=
        SnapOffsetToTexelLattice(out_light_view[12], world_units_per_texel) / ortho_size;
    out_light_proj[13] +=
        SnapOffsetToTexelLattice(out_light_view[13], world_units_per_texel) / ortho_size;
}

bool ShadowRenderData::PrepareShadowPass(const float* camera_mtx,
                                         const float* proj_mtx,
                                         float cam_near,
                                         float cam_far) {
    if (!enabled_ || type_ != ShadowType::ShadowMap) return false;
    // Ook zonder casters wordt de kaart opgebouwd (en leeg gewist): de far-cascade kan dan nog
    // schaduw geven in een gebied waar de near-kaart niets heeft.
    if (!shadow_map_valid_) {
        return false;
    }

    BuildLightMatrices(camera_mtx, proj_mtx, cam_near, cam_far,
                       light_view_, light_proj_);

    float light_view_proj_clip[16];
    bx::mtxMul(light_view_proj_clip, light_view_, light_proj_);
    float clip_to_texture[16];
    BuildShadowClipToTextureMatrix(clip_to_texture);
    bx::mtxMul(light_view_proj_, light_view_proj_clip, clip_to_texture);
    return true;
}

void ShadowRenderData::BeginShadowDepthPass(std::uint8_t view_id) {
    if (!shadow_map_valid_) {
        return;
    }

    const auto w = static_cast<std::uint16_t>(resolution_);
    const auto h = static_cast<std::uint16_t>(resolution_);

    bgfx::setViewName(view_id, "shadow_map");

    bgfx::setViewMode(view_id, bgfx::ViewMode::Default);
    bgfx::setViewClear(view_id,
                       BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                       0x00000000, 1.0f, 0);
    bgfx::setViewRect(view_id, 0, 0, w, h);
    bgfx::setViewFrameBuffer(view_id, backend_->shadow_fbo);
    bgfx::setViewTransform(view_id, light_view_, light_proj_);
    bgfx::setViewScissor(view_id, 0, 0, w, h);

    bgfx::touch(view_id);
}

void ShadowRenderData::SetEnabled(bool enabled) { enabled_ = enabled; }
bool ShadowRenderData::IsEnabled() const { return enabled_; }

std::string ShadowRenderData::GetQualityName(ShadowQuality q) {
    switch (q) {
        case ShadowQuality::Off:    return "Off";
        case ShadowQuality::Low:    return "Low";
        case ShadowQuality::Medium: return "Medium";
        case ShadowQuality::High:   return "High";
        case ShadowQuality::Ultra:  return "Ultra";
    }
    return "Unknown";
}

void ShadowRenderData::Reset() {
    casters_.clear();
    quality_    = ShadowQuality::Medium;
    type_       = ShadowType::Blob;
    resolution_ = 1024;
    distance_   = 40.0f;
    bias_       = 0.005f;
    lightX_     = 0.0f;
    lightY_     = -1.0f;
    lightZ_     = 0.0f;
    center_valid_ = false;
    enabled_    = true;
    DestroyShadowMap();
}

}
