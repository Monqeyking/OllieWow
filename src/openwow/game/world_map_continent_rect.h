#pragma once

#include <cstdint>

namespace openwow::game {

struct WorldMapContinentRect {
  float left, top, right, bottom;
};

// Classic world-sheet tile projection; matches Benilla map_proj::continent_sheet_rect.
// WorldMapContinent bounds are tile indices, not world-space coordinates.
inline WorldMapContinentRect ProjectWorldMapContinentRect(
    std::uint32_t left, std::uint32_t right, std::uint32_t top,
    std::uint32_t bottom, float offset_x, float offset_y, float scale) {
  const float xoff = 31.3125f - scale * 32.0f + offset_x;
  const float yoff = 20.875f - scale * 32.0f + offset_y;
  return {(static_cast<float>(left) * scale + xoff) / 62.625f,
          (static_cast<float>(top) * scale + yoff) / 41.75f,
          ((static_cast<float>(right) + 1.0f) * scale + xoff) / 62.625f,
          ((static_cast<float>(bottom) + 1.0f) * scale + yoff) / 41.75f};
}

} // namespace openwow::game
