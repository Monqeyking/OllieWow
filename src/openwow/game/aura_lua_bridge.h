#pragma once

#include "openwow/game/object_guid.h"

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

namespace openwow::game {

class WorldSession;

struct AuraQueryResult {
  std::string name;
  std::string rank;
  std::string icon;
  std::uint32_t count{0};
  std::string debuffType;
  float duration{0.0f};
  double expirationTime{0.0};
  float remainingTime{0.0f};
  std::string caster;
  ObjectGuid casterGuid;
  bool canStealOrPurge{false};
  bool shouldConsolidate{false};
  std::uint32_t spellId{0};
  // Tracker-slot van de aura. Uniek per aura en onafhankelijk van het filter,
  // zodat de 1.12 GetPlayerBuff-familie hem als handle kan gebruiken.
  std::uint8_t slot{0};
};

struct WeaponEnchantResult {
  bool hasMainHand{false};
  float mainHandExpiration{0.0f};
  int mainHandCharges{0};
  bool hasOffHand{false};
  float offHandExpiration{0.0f};
  int offHandCharges{0};
};

class AuraLuaBridge {
 public:
  static AuraLuaBridge& Get();

  [[nodiscard]] std::optional<AuraQueryResult> GetUnitBuff(
      const WorldSession& session, const ObjectGuid& unitGuid,
      std::uint32_t index) const;

  [[nodiscard]] std::optional<AuraQueryResult> GetUnitDebuff(
      const WorldSession& session, const ObjectGuid& unitGuid,
      std::uint32_t index) const;

  [[nodiscard]] std::optional<AuraQueryResult> GetUnitAura(
      const WorldSession& session, const ObjectGuid& unitGuid,
      std::uint32_t index,
      const std::string& filter) const;

  [[nodiscard]] std::optional<AuraQueryResult> FindUnitAura(
      const WorldSession& session, const ObjectGuid& unitGuid,
      const std::string& name,
      const std::string& rank, const std::string& filter) const;

  [[nodiscard]] std::optional<AuraQueryResult> UnitBuff(
      const WorldSession& session, const ObjectGuid& unitGuid,
      std::uint32_t index) const;

  [[nodiscard]] std::optional<AuraQueryResult> UnitDebuff(
      const WorldSession& session, const ObjectGuid& unitGuid,
      std::uint32_t index) const;

  void CancelUnitBuff(const WorldSession& session,
                      const ObjectGuid& unitGuid,
                      std::uint32_t index);

  // 1.12 `GetPlayerBuff(slot, buffFilter)`: `slot` is 0-based binnen de
  // gefilterde lijst (BuffFrame.xml declareert BuffButton0 met id="0"; pfUI
  // zet PLAYER_BUFF_START_ID = -1 met buttons vanaf 1). Bewust NIET via
  // SpellPassesScriptVisibilityFilter: dat is een 3.3.5-regel die auras
  // verbergt die de 1.12-buffbalk gewoon hoort te tonen.
  [[nodiscard]] std::optional<AuraQueryResult> GetPlayerBuffByPosition(
      WorldSession& session, std::uint32_t position,
      const std::string& filter) const;

  // Companionfuncties van GetPlayerBuff krijgen alleen de handle; de
  // tracker-slot is dan rechtstreeks op te zoeken, zonder filter.
  [[nodiscard]] std::optional<AuraQueryResult> GetPlayerAuraByTrackerSlot(
      WorldSession& session, std::uint32_t slot) const;

  // Annuleert de aura op die handle: stuurt CMSG_CANCEL_AURA en haalt hem
  // lokaal weg, met dezelfde positiviteitscheck als CancelUnitBuff.
  void CancelPlayerBuff(WorldSession& session, std::uint32_t slot) const;

  [[nodiscard]] WeaponEnchantResult GetWeaponEnchantInfo() const;

  void SetMainHandEnchant(float expiration, int charges);
  void SetOffHandEnchant(float expiration, int charges);
  void ClearWeaponEnchants();

 private:
  AuraLuaBridge() = default;

  bool has_mh_enchant_{false};
  float mh_expiration_{0.0f};
  int mh_charges_{0};
  bool has_oh_enchant_{false};
  float oh_expiration_{0.0f};
  int oh_charges_{0};
};

}
