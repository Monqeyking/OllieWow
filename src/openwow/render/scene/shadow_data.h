#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace bgfx {
struct Encoder;
}

namespace openwow::render {

enum class ShadowQuality : std::uint8_t {
    Off    = 0,
    Low    = 1,
    Medium = 2,
    High   = 3,
    Ultra  = 4,
};

enum class ShadowType : std::uint8_t {
    Blob      = 0,
    ShadowMap = 1,
};

struct ShadowCasterEntry {
    std::uint32_t entityId = 0;
    float         worldX   = 0.0f;
    float         worldY   = 0.0f;
    float         worldZ   = 0.0f;
    float         radius   = 1.0f;
    float         height   = 2.0f;
    bool          isValid  = true;
};

class ShadowRenderData {
public:
    ShadowRenderData();
    ~ShadowRenderData();

    ShadowRenderData(const ShadowRenderData&) = delete;
    ShadowRenderData& operator=(const ShadowRenderData&) = delete;

    void          SetQuality(ShadowQuality q);
    [[nodiscard]] ShadowQuality GetQuality() const;

    void          SetType(ShadowType t);
    [[nodiscard]] ShadowType GetType() const;

    void          SetShadowMapResolution(std::uint32_t res);
    [[nodiscard]] std::uint32_t GetShadowMapResolution() const;

    void          SetShadowDistance(float dist);
    [[nodiscard]] float GetShadowDistance() const;

    void          SetShadowBias(float bias);
    [[nodiscard]] float GetShadowBias() const;

    void          SetSplitLambda(float lambda);
    [[nodiscard]] float GetSplitLambda() const;

    bool CreateShadowMap();

    void DestroyShadowMap();

    [[nodiscard]] bool IsShadowMapValid() const;

    [[nodiscard]] const float* GetLightViewProj() const { return light_view_proj_; }
    [[nodiscard]] const float *GetLightView() const {
      return light_view_;
    }

    [[nodiscard]] const float *GetLightProj() const {
      return light_proj_;
    }

    void BindShadowState(bgfx::Encoder* encoder = nullptr) const;

    // Verre cascade: een tweede, grovere kaart die de ontvangers voorbij de near-kaart lezen
    // (slot 6). nullptr = geen verre kaart; BindShadowState zet dan sterkte 0 voor de far-kaart.
    void SetFarCascade(const ShadowRenderData* far_cascade) noexcept { far_cascade_ = far_cascade; }
    // Vaste kaartstraal (yd) in plaats van de straal uit de schaduwafstand; 0 = uit.
    void SetRadiusOverride(float radius) noexcept { radius_override_ = std::max(radius, 0.0f); }
    // Zwaardere diepte-bias voor de grove far-kaart (meer wereldeenheden per texel).
    void SetBiasScale(float scale) noexcept { bias_scale_ = std::max(scale, 0.0f); }
    // Debugmodus voor de ontvangers (u_shadowParamsFar.w): 0 uit, 1 far, 2 near, 3 dekking.
    void SetDebugMode(int mode) noexcept { debug_mode_ = mode; }
    // Rooster (yd) waaraan het kaartmiddelpunt vastzit en de dode zone waarbinnen het blijft staan.
    void SetCenterGrid(float grid, float hold) noexcept {
        center_grid_ = std::max(grid, 1.0f);
        center_hold_ = std::max(hold, 0.0f);
    }

    void AddCaster(ShadowCasterEntry entry);
    void RemoveCaster(std::uint32_t entityId);
    void SetCasters(std::span<const ShadowCasterEntry> casters);
    void ClearCasters() noexcept;

    [[nodiscard]] std::vector<ShadowCasterEntry> GetCasters() const;
    [[nodiscard]] std::uint32_t GetCasterCount() const;

    [[nodiscard]] std::vector<ShadowCasterEntry> GetCastersInRange(float x, float y, float z,
                                                                   float range) const;

    void SetLightDirection(float x, float y, float z);

    struct LightDir { float x, y, z; };
    [[nodiscard]] LightDir GetLightDirection() const;

    // De straal (yd) van de stabiele schaduwkaart voor een gevraagde schaduwafstand.
    // Eerder werd de kaart om de hele camerakegel gespannen (tot de farclip, 777 yd):
    // ~1 yd per texel, en een middelpunt dat met de camerapitch meebewoog.
    [[nodiscard]] static float RadiusForDistance(float distance) noexcept;
    [[nodiscard]] float GetShadowRadius() const noexcept { return radius_; }
    // Camerapositie en kijkrichting in wereldruimte; de kaart wordt hieromheen gelegd.
    void SetCameraAnchor(const float position[3], const float forward[3]) noexcept {
        for (int i = 0; i < 3; ++i) { anchor_pos_[i] = position[i]; anchor_fwd_[i] = forward[i]; }
    }
    // Het middelpunt van de laatst opgebouwde kaart (wereldruimte).
    [[nodiscard]] const float* GetShadowCenter() const noexcept { return center_; }

    bool PrepareShadowPass(const float* camera_mtx,
                           const float* proj_mtx,
                           float cam_near,
                           float cam_far);

    void BeginShadowDepthPass(std::uint8_t view_id);

    // Schaduwsterkte 0..1 (u_shadowParams.z): 0 = geen schaduw, 1 = volledig.
    void          SetStrength(float strength) { strength_ = std::clamp(strength, 0.0f, 1.0f); }
    [[nodiscard]] float GetStrength() const noexcept { return strength_; }

    void          SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;

    [[nodiscard]] static std::string GetQualityName(ShadowQuality q);

    void Reset();

private:
    struct BackendResources;

    void BuildLightMatrices(const float* camera_mtx,
                            const float* proj_mtx,
                            float cam_near,
                            float cam_far,
                            float out_light_view[16],
                            float out_light_proj[16]);

    std::vector<ShadowCasterEntry> casters_;

    ShadowQuality quality_   = ShadowQuality::Medium;
    ShadowType    type_      = ShadowType::Blob;
    std::uint32_t resolution_ = 1024;
    float         distance_  = 40.0f;
    float         bias_      = 0.005f;
    float         split_lambda_ = 0.5f;
    float         radius_    = 80.0f;
    float         depth_range_ = 320.0f;
    float         center_[3]{};
    bool          center_valid_ = false;
    float         anchor_pos_[3]{};
    float         anchor_fwd_[3]{0.0f, 1.0f, 0.0f};

    float lightX_    = 0.0f;
    float lightY_    = -1.0f;
    float lightZ_    = 0.0f;

    std::unique_ptr<BackendResources> backend_;
    float light_view_[16]{};
    float light_proj_[16]{};
    float light_view_proj_[16]{};

    float strength_     = 1.0f;
    const ShadowRenderData* far_cascade_ = nullptr;
    float radius_override_ = 0.0f;
    float bias_scale_ = 1.0f;
    int debug_mode_ = 0;
    float center_grid_ = 32.0f;
    float center_hold_ = 28.0f;
    bool enabled_       = true;
    bool shadow_map_valid_ = false;
};

}
