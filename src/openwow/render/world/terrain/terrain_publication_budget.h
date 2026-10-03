#pragma once

#include <chrono>
#include <cstddef>

namespace openwow::render {

// Soft budget: a unit may overrun the deadline; the next unit must not start.
// Taking time explicitly makes the admission rule deterministic in offline tests.
class TerrainPublicationBudget {
 public:
  using Clock = std::chrono::steady_clock;

  constexpr TerrainPublicationBudget(Clock::time_point deadline, std::size_t max_units) noexcept
      : deadline_(deadline), remaining_(max_units) {}

  [[nodiscard]] constexpr bool TryBeginUnit(Clock::time_point now) noexcept {
    if (remaining_ == 0u || now >= deadline_) {
      return false;
    }
    --remaining_;
    return true;
  }

 private:
  Clock::time_point deadline_;
  std::size_t remaining_;
};

}  // namespace openwow::render
