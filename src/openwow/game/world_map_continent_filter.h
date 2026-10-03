#pragma once

#include "openwow/data/formats/dbc/dbc_entries_extended.h"

#include <cmath>

namespace openwow::game {

// Turtle includes AreaID-zero instance placeholders and a World sheet.
// Only named, nondegenerate overview rectangles belong in the continent list.
// Preserve DBC file order; do not infer continent identity from fixed IDs.
inline bool IsWorldMapContinentOverview(
    const openwow::data::dbc::WorldMapAreaEntry& area) {
  return area.area_id == 0 && !area.name.empty() &&
         std::isfinite(area.loc_left) && std::isfinite(area.loc_right) &&
         std::isfinite(area.loc_top) && std::isfinite(area.loc_bottom) &&
         std::fabs(area.loc_left - area.loc_right) > 0.001f &&
         std::fabs(area.loc_top - area.loc_bottom) > 0.001f;
}

} // namespace openwow::game
