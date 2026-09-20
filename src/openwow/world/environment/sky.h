#pragma once

#include <array>
#include <cstdint>

namespace openwow::world {

inline constexpr float kDefaultSceneVisibility = 0.5f;

enum class SkyColorSlot : std::uint8_t {
  kGlobalAmbient = 0,
  kGlobalDiffuse = 1,
  kSunColor = 2,
  kSkyTop = 3,
  kSkyMiddle = 4,
  kSkyBand1 = 5,
  kSkyBand2 = 6,
  kSkySmog = 7,
  kSkyFog = 8,
  kSunHalo = 9,
  kCloudEdge = 10,
  kCloudColor = 11,
  kCloudHilight = 12,
  kSunGlow = 13,
  kWaterDark = 14,
  kWaterLight = 15,
  kShadow = 16,
  kFogEnd = 17,
  kCount = 18,
};

[[nodiscard]] inline constexpr std::uint32_t LightIntBandOffsetForSkyColorSlot(
    const SkyColorSlot slot) noexcept {
  switch (slot) {
  case SkyColorSlot::kGlobalAmbient:
    return 1u;
  case SkyColorSlot::kGlobalDiffuse:
    return 0u;
  case SkyColorSlot::kSunColor:
    // Rij 9 is de zon; rij 8 is de shadow-opacity-slot (vlak grijs). Zie
    // benilla-formats/src/light.rs:15-16 ("rows 2-6 = the 5 sky stops, 7 = fog,
    // 9 = sun ... row 8 is flat gray = shadow-opacity slot, not a sun").
    return 9u;
  case SkyColorSlot::kSkyTop:
    return 2u;
  case SkyColorSlot::kSkyMiddle:
    return 3u;
  case SkyColorSlot::kSkyBand1:
    return 4u;
  case SkyColorSlot::kSkyBand2:
    return 5u;
  case SkyColorSlot::kSkySmog:
    return 6u;
  case SkyColorSlot::kSkyFog:
    return 7u;
  default:
    return static_cast<std::uint32_t>(slot);
  }
}

[[nodiscard]] inline constexpr std::uint32_t LightIntBandIdForSkyColorSlot(
    const std::uint32_t light_params_id, const SkyColorSlot slot) noexcept {
  return light_params_id * static_cast<std::uint32_t>(SkyColorSlot::kCount) -
         17u + LightIntBandOffsetForSkyColorSlot(slot);
}

struct SkyColors {
  std::array<std::uint32_t, 18> colors{};
  float fog_distance{500.0f};
  float fog_multiplier{1.0f};
  float highlight_sky{0.0f};

  float scene_visibility{kDefaultSceneVisibility};

  float water_shallow_alpha{0.5f};
  float water_deep_alpha{1.0f};
  float ocean_shallow_alpha{0.75f};
  float ocean_deep_alpha{1.0f};
};

}
