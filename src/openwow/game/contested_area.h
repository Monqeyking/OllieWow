
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace openwow::data::dbc {
class DbcLoader;
}

namespace openwow::game {

class CGPlayer_C;

enum class ZonePvPType : std::uint8_t {
  Friendly  = 0,
  Hostile   = 1,
  Contested = 2,
  Sanctuary = 3,
  FFA       = 4,
  Combat    = 5,
};

struct ZonePvPInfo {
  bool available{false};
  ZonePvPType type{ZonePvPType::Contested};
  bool is_arena{false};
  bool has_faction_name{false};
  std::string faction_name;
};

// Vanilla territory compares the parent-zone owner mask, friend before enemy.
inline ZonePvPType ResolveClassicZonePvpType(std::uint32_t owner,
                                           std::uint32_t friends,
                                           std::uint32_t enemies) {
  if ((owner & friends) != 0u) return ZonePvPType::Friendly;
  if ((owner & enemies) != 0u) return ZonePvPType::Hostile;
  return ZonePvPType::Contested;
}

[[nodiscard]] ZonePvPInfo ResolveClassicZonePvpInfo(
    const openwow::data::dbc::DbcLoader& dbc,
    const CGPlayer_C* active_player,
    std::uint32_t zone_id,
    std::uint32_t sub_zone_id);

class ContestedAreaTracker {
 public:

  void SetCurrentZonePvPType(ZonePvPType type) { current_type_ = type; }
  [[nodiscard]] ZonePvPType GetCurrentZonePvPType() const {
    return current_type_;
  }

  [[nodiscard]] bool IsContested() const {
    return current_type_ == ZonePvPType::Contested;
  }
  [[nodiscard]] bool IsFriendly() const {
    return current_type_ == ZonePvPType::Friendly;
  }
  [[nodiscard]] bool IsHostile() const {
    return current_type_ == ZonePvPType::Hostile;
  }
  [[nodiscard]] bool IsSanctuary() const {
    return current_type_ == ZonePvPType::Sanctuary;
  }
  [[nodiscard]] bool IsCombatZone() const {
    return current_type_ == ZonePvPType::Combat;
  }

  void RegisterZone(std::uint32_t zoneId, ZonePvPType type,
                    const std::string& name);

  [[nodiscard]] ZonePvPType GetZonePvPType(std::uint32_t zoneId) const;
  [[nodiscard]] std::string GetZoneName(std::uint32_t zoneId) const;

  [[nodiscard]] static std::string GetPvPTypeName(ZonePvPType type);

  [[nodiscard]] static std::uint32_t GetPvPTypeColor(ZonePvPType type);

  void SetPlayerFaction(std::uint32_t factionId) {
    player_faction_ = factionId;
  }
  [[nodiscard]] std::uint32_t GetPlayerFaction() const {
    return player_faction_;
  }

  [[nodiscard]] std::uint32_t GetRegisteredZoneCount() const {
    return static_cast<std::uint32_t>(zones_.size());
  }

  void Clear();

 private:
  struct ZoneEntry {
    ZonePvPType type{ZonePvPType::Friendly};
    std::string name;
  };

  ZonePvPType current_type_{ZonePvPType::Friendly};
  std::uint32_t player_faction_{0};
  std::unordered_map<std::uint32_t, ZoneEntry> zones_;
};

}
