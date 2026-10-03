
#include "openwow/game/contested_area.h"

#include "openwow/data/formats/dbc/dbc_loader.h"
#include "openwow/game/objects/cgplayer.h"

namespace openwow::game {

namespace {

std::string ResolveFactionGroupName(
    const openwow::data::dbc::DbcLoader& dbc,
    const std::uint32_t faction_group_mask) {
  for (const auto& entry : dbc.faction_group()) {
    if (entry.mask_id >= 32u || entry.name.empty()) {
      continue;
    }
    if ((faction_group_mask & (1u << entry.mask_id)) != 0u) {
      return std::string(entry.name);
    }
  }
  return {};
}

}

ZonePvPInfo ResolveClassicZonePvpInfo(
    const openwow::data::dbc::DbcLoader& dbc,
    const CGPlayer_C* const active_player,
    const std::uint32_t zone_id,
    const std::uint32_t sub_zone_id) {
  const auto* const leaf = dbc.area_table().LookupEntry(sub_zone_id);
  const auto* const zone = dbc.area_table().LookupEntry(zone_id);
  const auto* const arena_area = leaf != nullptr ? leaf : zone;
  ZonePvPInfo result;
  result.is_arena = arena_area != nullptr &&
      (arena_area->flags & openwow::data::dbc::kAreaFlagArena) != 0u;
  if (active_player == nullptr || zone == nullptr) return result;
  const auto* const faction_template = dbc.faction_template().LookupEntry(
      active_player->State().GetFactionTemplate());
  if (faction_template == nullptr) return result;

  result.type = ResolveClassicZonePvpType(zone->faction_group_mask,
      faction_template->friend_group, faction_template->enemy_group);
  result.faction_name = ResolveFactionGroupName(dbc, zone->faction_group_mask);
  result.has_faction_name = !result.faction_name.empty();
  // A friendly/hostile result without its DBC faction name cannot satisfy
  // ZoneText.xml format(..., factionName). Do not manufacture a fallback name.
  result.available = result.type == ZonePvPType::Contested || result.has_faction_name;
  return result;
}

void ContestedAreaTracker::RegisterZone(std::uint32_t zoneId,
                                        ZonePvPType type,
                                        const std::string& name) {
  zones_[zoneId] = ZoneEntry{type, name};
}

ZonePvPType ContestedAreaTracker::GetZonePvPType(
    std::uint32_t zoneId) const {
  auto it = zones_.find(zoneId);
  if (it != zones_.end()) return it->second.type;

  return ZonePvPType::Friendly;
}

std::string ContestedAreaTracker::GetZoneName(std::uint32_t zoneId) const {
  auto it = zones_.find(zoneId);
  if (it != zones_.end()) return it->second.name;
  return {};
}

std::string ContestedAreaTracker::GetPvPTypeName(ZonePvPType type) {
  switch (type) {
    case ZonePvPType::Friendly:  return "Friendly";
    case ZonePvPType::Hostile:   return "Hostile";
    case ZonePvPType::Contested: return "Contested";
    case ZonePvPType::Sanctuary: return "Sanctuary";
    case ZonePvPType::FFA:       return "FFA";
    case ZonePvPType::Combat:    return "Combat";
  }
  return "Unknown";
}

std::uint32_t ContestedAreaTracker::GetPvPTypeColor(ZonePvPType type) {
  switch (type) {
    case ZonePvPType::Friendly:  return 0xFF00FF00;
    case ZonePvPType::Hostile:   return 0xFFFF0000;
    case ZonePvPType::Contested: return 0xFFFF8000;
    case ZonePvPType::Sanctuary: return 0xFF69CCF0;
    case ZonePvPType::FFA:       return 0xFFFF0000;
    case ZonePvPType::Combat:    return 0xFFFF0000;
  }
  return 0xFFFFFFFF;
}

void ContestedAreaTracker::Clear() {
  current_type_    = ZonePvPType::Friendly;
  player_faction_  = 0;
  zones_.clear();
}

}
