#include "openwow/render/world/terrain/terrain_publication_budget.h"

#include <chrono>
#include <cstddef>
#include <iostream>

namespace {
using Budget = openwow::render::TerrainPublicationBudget;
using Clock = Budget::Clock;
using namespace std::chrono_literals;

constexpr bool AdmissionContract() {
  const Clock::time_point start{};
  Budget zero(start + 1ms, 0u);
  Budget expired(start, 16u);
  Budget count_limited(start + 10ms, 2u);
  Budget time_limited(start + 2ms, 16u);
  return !zero.TryBeginUnit(start) && !expired.TryBeginUnit(start) &&
         count_limited.TryBeginUnit(start) && count_limited.TryBeginUnit(start + 1ms) &&
         !count_limited.TryBeginUnit(start + 2ms) &&
         time_limited.TryBeginUnit(start) && !time_limited.TryBeginUnit(start + 2ms) &&
         !time_limited.TryBeginUnit(start + 3ms);
}
static_assert(AdmissionContract());

// Synthetic units deliberately overrun the deadline. This verifies only the shared admission
// helper, not BGFX lifetime or rendering: those require the parent integration/runtime checks.
bool SoftDeadlineAndFrameReset() {
  const Clock::time_point start{};
  Budget first_frame(start + 1ms, 16u);
  if (!first_frame.TryBeginUnit(start)) {
    return false;
  }
  // Simulated indivisible driver operation took 5ms; no second operation may start.
  if (first_frame.TryBeginUnit(start + 5ms)) {
    return false;
  }
  Budget next_frame(start + 10ms, 1u);
  return next_frame.TryBeginUnit(start + 5ms) &&
         !next_frame.TryBeginUnit(start + 5ms);
}

bool UnitCountAcrossFrames(const std::size_t max_units) {
  constexpr std::size_t total_units = 3u + 256u + 256u + 256u + 5u;
  const Clock::time_point start{};
  std::size_t completed = 0u;
  std::size_t frames = 0u;
  while (completed < total_units) {
    Budget budget(start + 1ms, max_units);
    std::size_t frame_units = 0u;
    while (completed < total_units && budget.TryBeginUnit(start)) {
      ++completed;
      ++frame_units;
    }
    if (frame_units == 0u || frame_units > max_units) {
      return false;
    }
    ++frames;
  }
  return completed == total_units && frames == (total_units + max_units - 1u) / max_units;
}
}  // namespace

int main() {
  if (!AdmissionContract() || !SoftDeadlineAndFrameReset() || !UnitCountAcrossFrames(16u) || !UnitCountAcrossFrames(32u)) {
    std::cerr << "terrain publication budget regression failed\n";
    return 1;
  }
  std::cout << "terrain publication budget regression passed\n";
  return 0;
}
