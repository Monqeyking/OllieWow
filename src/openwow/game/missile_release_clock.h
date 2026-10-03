#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace openwow::game {

// Millisecond ticks use modular ordering: deadlines and observations must be
// separated by less than half the uint32_t range (about 24.8 days).
struct MissileReleaseClock {
  std::uint32_t go_tick{0};
  std::uint32_t deadline_tick{0};

  [[nodiscard]] static MissileReleaseClock FromGo(
      std::uint32_t tick, const std::array<float, 3>& src,
      const std::array<float, 3>& dst, float speed) noexcept {
    MissileReleaseClock clock{tick, tick};
    if (!(speed > 0.0f) || !std::isfinite(speed)) return clock;

    double distance_squared = 0.0;
    for (std::size_t axis = 0; axis < src.size(); ++axis) {
      if (!std::isfinite(src[axis]) || !std::isfinite(dst[axis]))
        return clock;
      const double delta = static_cast<double>(dst[axis]) - src[axis];
      distance_squared += delta * delta;
    }
    // Round up so a nonzero journey never expires before its GO travel time.
    const double milliseconds =
        std::ceil(std::sqrt(distance_squared) * 1000.0 / speed);
    constexpr std::uint32_t kMaximumDuration = 0x7fffffffu;
    const auto duration = milliseconds >= kMaximumDuration
                              ? kMaximumDuration
                              : static_cast<std::uint32_t>(milliseconds);
    clock.deadline_tick += duration;
    return clock;
  }

  // Local Source/src/game/Spells/Spell.cpp::AddUnitTarget (1061-1072):
  // center distance (no object radius), minimum 5 yards, floor milliseconds.
  // Use authoritative GO object positions, never cached M2 attachment roots.
  // Ground/timed trajectories keep their separate FromGo policy.
  [[nodiscard]] static MissileReleaseClock FromUnitGo(
      std::uint32_t tick, const std::array<float, 3>& src,
      const std::array<float, 3>& dst, float speed,
      bool self_target = false) noexcept {
    MissileReleaseClock clock{tick, tick};
    if (self_target || !(speed > 0.0f) || !std::isfinite(speed)) return clock;
    double distance_squared = 0.0;
    for (std::size_t axis = 0; axis < src.size(); ++axis) {
      if (!std::isfinite(src[axis]) || !std::isfinite(dst[axis])) return clock;
      const double delta = static_cast<double>(dst[axis]) - src[axis];
      distance_squared += delta * delta;
    }
    const double distance = std::sqrt(distance_squared);
    const double milliseconds = std::floor((distance < 5.0 ? 5.0 : distance) * 1000.0 / speed);
    constexpr std::uint32_t kMaximumDuration = 0x7fffffffu;
    clock.deadline_tick += milliseconds >= kMaximumDuration ? kMaximumDuration
        : static_cast<std::uint32_t>(milliseconds);
    return clock;
  }

  [[nodiscard]] float RemainingSeconds(std::uint32_t now) const noexcept {
    if (Expired(now)) return 0.0f;
    return static_cast<float>(deadline_tick - now) * 0.001f;
  }

  [[nodiscard]] bool Expired(std::uint32_t now) const noexcept {
    // Unsigned subtraction avoids implementation-defined signed conversion.
    return (now - deadline_tick) < 0x80000000u;
  }
};

// Use remaining time at the START of the frame. Target motion changes the
// destination, never the GO deadline; the last frame reaches that destination.
[[nodiscard]] inline float MissileHomingFraction(
    float frame_seconds, float remaining_seconds) noexcept {
  if (!(frame_seconds > 0.0f) || std::isnan(remaining_seconds)) return 0.0f;
  if (!(remaining_seconds > 0.0f) || frame_seconds >= remaining_seconds)
    return 1.0f;
  return frame_seconds / remaining_seconds;
}

// Payload ownership stays with the caller. GO queues an identity; a release
// marker, animation completion, or no-animation path consumes it exactly once.
struct MissileReleaseGate {
  std::uint64_t serial{0};
  std::uint16_t animation{0};
  bool released{false};
  bool cancelled{false};

  [[nodiscard]] bool Matches(std::uint64_t event_serial,
                             std::uint16_t event_animation) const noexcept {
    return !released && !cancelled && serial != 0 &&
           serial == event_serial && animation == event_animation;
  }

  [[nodiscard]] bool TryRelease(std::uint64_t event_serial,
                                std::uint16_t event_animation) noexcept {
    if (!Matches(event_serial, event_animation)) return false;
    released = true;
    return true;
  }

  void Reset() noexcept {
    serial = 0;
    animation = 0;
    released = false;
    cancelled = false;
  }
};

} // namespace openwow::game
