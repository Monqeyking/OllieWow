#pragma once

#include <atomic>

namespace openwow::render {
class ShadowRenderData;
}

namespace openwow::render::m2 {

// De schaduwkaart waaruit M2-draws de zon-schaduw lezen. Leeg (nullptr) tijdens de schaduwpas
// zelf (een kaart wordt niet gelezen terwijl hij beschreven wordt) en als schaduwen uit staan.
// ShadowPresentationRuntime zet hem; M2SkinnedMesh bindt hem per draw.
[[nodiscard]] inline std::atomic<const ShadowRenderData *> &ShadowReceiverSlot() noexcept {
  static std::atomic<const ShadowRenderData *> slot{nullptr};
  return slot;
}

}  // namespace openwow::render::m2
