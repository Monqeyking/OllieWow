#pragma once

#include <bgfx/bgfx.h>

#include <atomic>
#include <cstdint>

namespace openwow::render {

// Vertaalt de Vanilla-opties "Trilinear Filtering" (CVar `trilinear`) en
// "Anisotropic Filtering" (CVar `anisotropic`, 1x..16x) naar bgfx-sampler-
// vlaggen voor wereldtextures (terrein, M2, WMO). Eerder werden die CVars wel
// geregistreerd maar door geen enkele renderer gelezen.
//
// bgfx kent maar twee anisotropieniveaus: uit (1x) of aan (max, 16x). Elke
// waarde boven 1x schakelt dus 16x in. Anisotropie impliceert trilinear.
struct WorldSamplerQualityState {
  std::atomic<bool> anisotropic{false};
  std::atomic<bool> trilinear{true};
};

[[nodiscard]] inline WorldSamplerQualityState& WorldSamplerQuality() noexcept {
  static WorldSamplerQualityState state;
  return state;
}

[[nodiscard]] inline std::uint32_t WorldSamplerQualityFlags() noexcept {
  const auto& state = WorldSamplerQuality();
  if (state.anisotropic.load(std::memory_order_relaxed)) {
    return BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC;
  }
  if (!state.trilinear.load(std::memory_order_relaxed)) {
    return BGFX_SAMPLER_MIP_POINT;
  }
  return 0u;
}

}
