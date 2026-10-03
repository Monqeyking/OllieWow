#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace openwow::render {

// Vanilla secondary owner (Benilla creature_anim/driver/wound.rs). This is
// deliberately not a primary AnimationState: no fallback or completion callback.
struct WoundSecondaryRequest {
  std::uint16_t animation_id{0};
  std::uint32_t duration_ms{0};
  std::uint64_t serial{0};
  bool masked{false};
  bool active{false};

  bool Trigger(std::uint16_t id, std::uint32_t span, bool mask, bool dead) {
    if (dead || span == 0u) return false;
    animation_id = id;
    duration_ms = span;
    masked = mask;
    active = true;
    if (++serial == 0u) serial = 1u;
    return true;
  }
  bool EvictForPrimaryRearm(bool primary_masked, bool zero_blend) {
    if (!active || zero_blend || masked != primary_masked) return false;
    active = false;
    if (++serial == 0u) serial = 1u;
    return true;
  }
  [[nodiscard]] bool operator==(const WoundSecondaryRequest &) const = default;
};

[[nodiscard]] inline bool WoundFullBody(std::uint16_t wound_id,
    std::uint32_t base_behavior, bool stationary_unmounted) {
  return (base_behavior >= 25u && base_behavior <= 29u) ||
         (wound_id == 8u && stationary_unmounted);
}

[[nodiscard]] inline std::uint16_t SelectMeleeWound(bool critical, bool engaged) {
  return critical ? 10u : (engaged ? 9u : 8u);
}

[[nodiscard]] inline float WoundSecondaryWeight(double elapsed_ms,
                                                 std::uint32_t duration_ms) {
  if (duration_ms == 0u) return 0.0f;
  const float t = static_cast<float>(std::clamp(
      1.0 - elapsed_ms / static_cast<double>(duration_ms), 0.0, 1.0));
  return (3.0f - 2.0f * t) * t * t * 0.75f;
}

[[nodiscard]] inline float WoundBlendComponent(float primary, float secondary, float weight) {
  return primary + (secondary - primary) * weight;
}

struct WoundSecondaryState {
  WoundSecondaryRequest request{};
  double elapsed_ms{0.0};
  bool active{false};

  // A stable published serial must not resurrect an expired secondary.
  bool Synchronize(const WoundSecondaryRequest &next) {
    if (request.serial == next.serial) return false;
    request = next;
    elapsed_ms = 0.0;
    active = next.active && next.duration_ms != 0u;
    return true;
  }
  void Update(float dt) {
    if (!active || !std::isfinite(dt) || dt <= 0.0f) return;
    elapsed_ms = std::min(elapsed_ms + static_cast<double>(dt) * 1000.0,
                          static_cast<double>(request.duration_ms));
    if (elapsed_ms >= request.duration_ms) active = false;
  }
  [[nodiscard]] std::uint32_t time_ms() const {
    return static_cast<std::uint32_t>(elapsed_ms);
  }
  [[nodiscard]] float weight() const {
    return active ? WoundSecondaryWeight(elapsed_ms, request.duration_ms) : 0.0f;
  }
};

} // namespace openwow::render
