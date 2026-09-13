#pragma once

#include "openwow/game/rune_handler.h"
#include "openwow/game/unit_defines.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace openwow::game {

[[nodiscard]] const char *PowerTypeToString(PowerType pt);

enum class UsabilityReason : std::uint8_t {
  kUsable = 0,
  kNotKnown,
  kOnCooldown,
  kNotEnoughPower,
  kLevelTooLow,
  kWrongShapeshift,
  kMissingEquippedItem,
  kOutOfRange,
  kCasterDead,
  kCasterMoving,
  kCasterStunned,
  kCasterSilenced,
  kCasterPacified,
  kSpellInProgress,
  kPassiveSpell,
  kNotInCorrectForm,
};

struct UsabilityResult {
  UsabilityReason reason = UsabilityReason::kUsable;
  bool is_usable = true;
  bool not_enough_power = false;
  bool out_of_range = false;
  std::uint32_t power_cost = 0;
  PowerType power_type = PowerType::kMana;

  [[nodiscard]] bool operator==(const UsabilityResult &o) const {
    return reason == o.reason && is_usable == o.is_usable;
  }
  [[nodiscard]] bool operator!=(const UsabilityResult &o) const {
    return !(*this == o);
  }
};

struct PlayerStateSnapshot {
  struct EquippedItemMetadata {
    bool present = false;
    std::uint32_t item_class = 0;
    std::uint32_t subclass = 0;
    std::uint32_t inventory_type = 0;
    bool passes_weapon_state_check = true;
  };

  std::uint32_t level = 1;
  bool is_dead = false;
  bool is_moving = false;
  bool is_stunned = false;
  bool is_silenced = false;
  bool is_pacified = false;
  bool is_casting = false;

  std::uint32_t mana = 0;
  std::uint32_t max_mana = 0;
  std::uint32_t rage = 0;
  std::uint32_t focus = 0;
  std::uint32_t energy = 0;
  std::uint32_t runic_power = 0;
  std::uint32_t health = 0;

  std::uint32_t shapeshift_form = 0;

  bool shapeshift_form_is_turn_sensitive = false;

  std::uint32_t shapeshift_form_flags = 0;

  bool has_aura_ignore_shapeshift = false;

  std::uint32_t equipped_item_class_mask = 0;
  std::uint32_t equipped_item_subclass_mask = 0;
  std::uint32_t equipped_item_inv_type_mask = 0;
  std::array<EquippedItemMetadata, 3> equipped_item_slots{};

  std::uint32_t combo_points = 0;

  struct ReadyRuneCounts {
    std::uint8_t blood = 0;
    std::uint8_t unholy = 0;
    std::uint8_t frost = 0;
    std::uint8_t death = 0;
  } ready_runes;
  bool has_rune_data = false;

  bool is_mounted = false;
  bool can_act_while_mounted = false;
  bool can_change_movement_direction = false;

  bool is_stealthed = false;
  bool is_channeling = false;
  bool is_auto_attacking = false;

  bool is_indoors = false;
  bool is_in_battleground = false;
  bool is_in_arena = false;

  std::int32_t sheathe_state = 0;

  bool is_daytime = true;

  bool is_in_area_group = true;

  bool has_aura_type_78 = false;

  std::uint32_t map_type = 0;

  std::uint8_t pet_tame_status_flags = 0;

  std::uint32_t reputation_standing = 0;
  std::uint32_t skill_value = 0;
};

struct SpellUsabilityInfo {
  std::uint32_t spell_id = 0;
  std::uint32_t base_level = 0;
  std::uint32_t spell_level = 0;
  std::uint32_t mana_cost = 0;
  std::uint32_t mana_cost_pct = 0;
  PowerType power_type = PowerType::kMana;
  RuneCost rune_cost{};
  bool has_rune_cost = false;
  bool is_passive = false;
  bool is_known = false;
  bool on_cooldown = false;
  bool on_gcd = false;

  std::uint32_t stances = 0;
  std::uint32_t stances_high = 0;
  std::uint32_t stances_not = 0;
  std::uint32_t stances_not_high = 0;

  std::int32_t equipped_item_class = -1;
  std::int32_t equipped_item_subclass_mask = 0;
  std::int32_t equipped_item_inv_type_mask = 0;

  std::uint32_t attributes = 0;

  std::uint32_t attributes_ex = 0;

  std::uint32_t attributes_ex2 = 0;

  std::uint32_t attributes_ex3 = 0;

  std::uint32_t attributes_ex5 = 0;

  std::uint32_t attributes_ex6 = 0;

  std::uint32_t spell_family_name = 0;
  std::array<std::uint32_t, 3> spell_family_flags{};
  std::uint32_t school_mask = 0;
  std::int32_t rune_cost_pct = 100;

  std::uint32_t aura_interrupt_flags = 0;

  std::uint32_t channel_interrupt_flags = 0;

  float max_range = 0.0f;
  float target_distance = -1.0f;

  bool usable_in_shapeshift = true;
  bool castable_while_mounted = false;
  bool castable_while_moving = false;
  bool has_target = false;
  bool has_caster_target = false;
  bool is_melee_range = false;

  std::uint32_t effect_count = 0;
  std::uint32_t effect_ids[3] = {};
  std::uint32_t effect_aura_ids[3] = {};
  std::uint32_t effect_mechanic_ids[3] = {};
  std::uint32_t aura_mechanic_ids[3] = {};

  std::uint32_t required_faction_id = 0;
  std::uint32_t required_reputation_level = 0;
  std::uint32_t required_skill_id = 0;
  std::uint32_t required_skill_value = 0;

  std::int32_t area_group_id = 0;
};

class SpellUsabilityChecker {
public:

  [[nodiscard]] static UsabilityResult ComputeUsability(const SpellUsabilityInfo &spell,
                                                        const PlayerStateSnapshot &player);

  [[nodiscard]] static bool CheckKnown(const SpellUsabilityInfo &spell);

  [[nodiscard]] static bool CheckCooldown(const SpellUsabilityInfo &spell);

  [[nodiscard]] static bool CheckPower(const SpellUsabilityInfo &spell,
                                       const PlayerStateSnapshot &player);

  [[nodiscard]] static std::uint32_t ComputePowerCost(const SpellUsabilityInfo &spell,
                                                      const PlayerStateSnapshot &player);

  [[nodiscard]] static std::uint32_t GetCurrentPower(PowerType type,
                                                     const PlayerStateSnapshot &player);

  [[nodiscard]] static bool CheckLevel(const SpellUsabilityInfo &spell,
                                       const PlayerStateSnapshot &player);

  [[nodiscard]] static bool CheckShapeshift(const SpellUsabilityInfo &spell,
                                            const PlayerStateSnapshot &player);

  [[nodiscard]] static bool CheckEquippedItem(const SpellUsabilityInfo &spell,
                                              const PlayerStateSnapshot &player);

  [[nodiscard]] static bool CheckRange(const SpellUsabilityInfo &spell);

  [[nodiscard]] static bool CheckCasterState(const SpellUsabilityInfo &spell,
                                             const PlayerStateSnapshot &player);

  enum class SpellCastError : std::uint32_t {
    // These values are translated to SpellCastResult by the cast path. Keep
    // the shared entries on the Vanilla 1.12.1 wire values; the old WotLK
    // numbering made stance/stealth failures resolve to unrelated messages.
    kNone = 0xFF,
    kAffectingCombat = 0,
    kAlreadyHaveCharm = 4,
    kAlreadyHaveSummon = 5,
    kCasterAuraState = 18,
    kCharmed = 20,
    kConfused = 22,
    kDontReport = 23,
    kEquippedItem = 24,
    kFleeing = 30,
    kIncorrectArea = 93,
    kItemNotReady = 40,
    kLineOfSight = 42,
    kMoving = 46,
    kNeedExoticAmmo = 49,
    kNeedMoreItems = 146,
    kNotHere = 53,
    kNotMounted = 57,
    kNotReady = 60,
    kNotShapeshift = 61,
    kNotStanding = 62,
    kNotUnsheathed = 65,
    kNoMountsAllowed = 75,
    kNoPet = 76,
    kNoPower = 77,
    kOnlyAbovewater = 80,
    kOnlyDaytime = 81,
    kOnlyIndoors = 82,
    kOnlyMounted = 83,
    kOnlyNighttime = 84,
    kOnlyOutdoors = 85,
    kOnlyShapeshift = 86,
    kOnlyStealthed = 87,
    kOnlyUnderwater = 88,
    kOutOfRange = 89,
    kRequiresArea = 93,
    kSilenced = 96,
    kSpellInProgress = 97,
    kSpellUnavailable = 99,
    kStunned = 100,
    kOnlyBattlegrounds = 135,
    kPreventedByMechanic = 141,
    kReputation = 143,
    // Vanilla has no arena/raid-instance result at these WotLK-only slots.
    kNotInArena = 145,
    kNotInRaidInstance = 145,
    kOnlyInArena = 145,
  };

  [[nodiscard]] static bool CheckStanceRequirement(
      const SpellUsabilityInfo &spell,
      const PlayerStateSnapshot &player,
      SpellCastError *out_error = nullptr);

  [[nodiscard]] static bool CheckStealthedRequirement(
      const SpellUsabilityInfo &spell,
      const PlayerStateSnapshot &player,
      SpellCastError *out_error = nullptr);

  [[nodiscard]] static bool CheckCasterRequirements(const SpellUsabilityInfo &spell,
                                                    const PlayerStateSnapshot &player,
                                                    SpellCastError *out_error = nullptr);
};

}
