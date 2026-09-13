
#include "openwow/game/spell_failure_names.h"
#include "openwow/game/spell_public_values.h"

#include <cstdio>

#include "openwow/game/inventory/items/item_definitions.h"
#include "openwow/core/localized_format.h"
#include "openwow/game/localization.h"
#include "openwow/ui/surfaces/game/runtime/system_message_dispatch.h"

namespace openwow::game {

const char* SpellFailedReasonToString(std::uint32_t reason) {
  static constexpr const char* const kVanillaNames[] = {
    "SPELL_FAILED_AFFECTING_COMBAT",
    "SPELL_FAILED_ALREADY_AT_FULL_HEALTH",
    "SPELL_FAILED_ALREADY_AT_FULL_POWER",
    "SPELL_FAILED_ALREADY_BEING_TAMED",
    "SPELL_FAILED_ALREADY_HAVE_CHARM",
    "SPELL_FAILED_ALREADY_HAVE_SUMMON",
    "SPELL_FAILED_ALREADY_OPEN",
    "SPELL_FAILED_AURA_BOUNCED",
    "SPELL_FAILED_AUTOTRACK_INTERRUPTED",
    "SPELL_FAILED_BAD_IMPLICIT_TARGETS",
    "SPELL_FAILED_BAD_TARGETS",
    "SPELL_FAILED_CANT_BE_CHARMED",
    "SPELL_FAILED_CANT_BE_DISENCHANTED",
    "SPELL_FAILED_CANT_BE_PROSPECTED",
    "SPELL_FAILED_CANT_CAST_ON_TAPPED",
    "SPELL_FAILED_CANT_DUEL_WHILE_INVISIBLE",
    "SPELL_FAILED_CANT_DUEL_WHILE_STEALTHED",
    "SPELL_FAILED_CANT_STEALTH",
    "SPELL_FAILED_CASTER_AURASTATE",
    "SPELL_FAILED_CASTER_DEAD",
    "SPELL_FAILED_CHARMED",
    "SPELL_FAILED_CHEST_IN_USE",
    "SPELL_FAILED_CONFUSED",
    "SPELL_FAILED_DONT_REPORT",
    "SPELL_FAILED_EQUIPPED_ITEM",
    "SPELL_FAILED_EQUIPPED_ITEM_CLASS",
    "SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND",
    "SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND",
    "SPELL_FAILED_ERROR",
    "SPELL_FAILED_FIZZLE",
    "SPELL_FAILED_FLEEING",
    "SPELL_FAILED_FOOD_LOWLEVEL",
    "SPELL_FAILED_HIGHLEVEL",
    "SPELL_FAILED_HUNGER_SATIATED",
    "SPELL_FAILED_IMMUNE",
    "SPELL_FAILED_INTERRUPTED",
    "SPELL_FAILED_INTERRUPTED_COMBAT",
    "SPELL_FAILED_ITEM_ALREADY_ENCHANTED",
    "SPELL_FAILED_ITEM_GONE",
    "SPELL_FAILED_ITEM_NOT_FOUND",
    "SPELL_FAILED_ITEM_NOT_READY",
    "SPELL_FAILED_LEVEL_REQUIREMENT",
    "SPELL_FAILED_LINE_OF_SIGHT",
    "SPELL_FAILED_LOWLEVEL",
    "SPELL_FAILED_LOW_CASTLEVEL",
    "SPELL_FAILED_MAINHAND_EMPTY",
    "SPELL_FAILED_MOVING",
    "SPELL_FAILED_NEED_AMMO",
    "SPELL_FAILED_NEED_AMMO_POUCH",
    "SPELL_FAILED_NEED_EXOTIC_AMMO",
    "SPELL_FAILED_NOPATH",
    "SPELL_FAILED_NOT_BEHIND",
    "SPELL_FAILED_NOT_FISHABLE",
    "SPELL_FAILED_NOT_HERE",
    "SPELL_FAILED_NOT_INFRONT",
    "SPELL_FAILED_NOT_IN_CONTROL",
    "SPELL_FAILED_NOT_KNOWN",
    "SPELL_FAILED_NOT_MOUNTED",
    "SPELL_FAILED_NOT_ON_TAXI",
    "SPELL_FAILED_NOT_ON_TRANSPORT",
    "SPELL_FAILED_NOT_READY",
    "SPELL_FAILED_NOT_SHAPESHIFT",
    "SPELL_FAILED_NOT_STANDING",
    "SPELL_FAILED_NOT_TRADEABLE",
    "SPELL_FAILED_NOT_TRADING",
    "SPELL_FAILED_NOT_UNSHEATHED",
    "SPELL_FAILED_NOT_WHILE_GHOST",
    "SPELL_FAILED_NO_AMMO",
    "SPELL_FAILED_NO_CHARGES_REMAIN",
    "SPELL_FAILED_NO_CHAMPION",
    "SPELL_FAILED_NO_COMBO_POINTS",
    "SPELL_FAILED_NO_DUELING",
    "SPELL_FAILED_NO_ENDURANCE",
    "SPELL_FAILED_NO_FISH",
    "SPELL_FAILED_NO_ITEMS_WHILE_SHAPESHIFTED",
    "SPELL_FAILED_NO_MOUNTS_ALLOWED",
    "SPELL_FAILED_NO_PET",
    "SPELL_FAILED_NO_POWER",
    "SPELL_FAILED_NOTHING_TO_DISPEL",
    "SPELL_FAILED_NOTHING_TO_STEAL",
    "SPELL_FAILED_ONLY_ABOVEWATER",
    "SPELL_FAILED_ONLY_DAYTIME",
    "SPELL_FAILED_ONLY_INDOORS",
    "SPELL_FAILED_ONLY_MOUNTED",
    "SPELL_FAILED_ONLY_NIGHTTIME",
    "SPELL_FAILED_ONLY_OUTDOORS",
    "SPELL_FAILED_ONLY_SHAPESHIFT",
    "SPELL_FAILED_ONLY_STEALTHED",
    "SPELL_FAILED_ONLY_UNDERWATER",
    "SPELL_FAILED_OUT_OF_RANGE",
    "SPELL_FAILED_PACIFIED",
    "SPELL_FAILED_POSSESSED",
    "SPELL_FAILED_REAGENTS",
    "SPELL_FAILED_REQUIRES_AREA",
    "SPELL_FAILED_REQUIRES_SPELL_FOCUS",
    "SPELL_FAILED_ROOTED",
    "SPELL_FAILED_SILENCED",
    "SPELL_FAILED_SPELL_IN_PROGRESS",
    "SPELL_FAILED_SPELL_LEARNED",
    "SPELL_FAILED_SPELL_UNAVAILABLE",
    "SPELL_FAILED_STUNNED",
    "SPELL_FAILED_TARGETS_DEAD",
    "SPELL_FAILED_TARGET_AFFECTING_COMBAT",
    "SPELL_FAILED_TARGET_AURASTATE",
    "SPELL_FAILED_TARGET_DUELING",
    "SPELL_FAILED_TARGET_ENEMY",
    "SPELL_FAILED_TARGET_ENRAGED",
    "SPELL_FAILED_TARGET_FRIENDLY",
    "SPELL_FAILED_TARGET_IN_COMBAT",
    "SPELL_FAILED_TARGET_IS_PLAYER",
    "SPELL_FAILED_TARGET_NOT_DEAD",
    "SPELL_FAILED_TARGET_NOT_IN_PARTY",
    "SPELL_FAILED_TARGET_NOT_LOOTED",
    "SPELL_FAILED_TARGET_NOT_PLAYER",
    "SPELL_FAILED_TARGET_NO_POCKETS",
    "SPELL_FAILED_TARGET_NO_WEAPONS",
    "SPELL_FAILED_TARGET_UNSKINNABLE",
    "SPELL_FAILED_THIRST_SATIATED",
    "SPELL_FAILED_TOO_CLOSE",
    "SPELL_FAILED_TOO_MANY_OF_ITEM",
    "SPELL_FAILED_TOTEMS",
    "SPELL_FAILED_TRAINING_POINTS",
    "SPELL_FAILED_TRY_AGAIN",
    "SPELL_FAILED_UNIT_NOT_BEHIND",
    "SPELL_FAILED_UNIT_NOT_INFRONT",
    "SPELL_FAILED_WRONG_PET_FOOD",
    "SPELL_FAILED_NOT_WHILE_FATIGUED",
    "SPELL_FAILED_TARGET_NOT_IN_INSTANCE",
    "SPELL_FAILED_NOT_WHILE_TRADING",
    "SPELL_FAILED_TARGET_NOT_IN_RAID",
    "SPELL_FAILED_DISENCHANT_WHILE_LOOTING",
    "SPELL_FAILED_PROSPECT_WHILE_LOOTING",
    "SPELL_FAILED_PROSPECT_NEED_MORE",
    "SPELL_FAILED_TARGET_FREEFORALL",
    "SPELL_FAILED_NO_EDIBLE_CORPSES",
    "SPELL_FAILED_ONLY_BATTLEGROUNDS",
    "SPELL_FAILED_TARGET_NOT_GHOST",
    "SPELL_FAILED_TOO_MANY_SKILLS",
    "SPELL_FAILED_TRANSFORM_UNUSABLE",
    "SPELL_FAILED_WRONG_WEATHER",
    "SPELL_FAILED_DAMAGE_IMMUNE",
    "SPELL_FAILED_PREVENTED_BY_MECHANIC",
    "SPELL_FAILED_PLAY_TIME",
    "SPELL_FAILED_REPUTATION",
    "SPELL_FAILED_MIN_SKILL",
    "SPELL_FAILED_UNKNOWN",
  };
  constexpr auto kNameCount = sizeof(kVanillaNames) / sizeof(kVanillaNames[0]);
  if (reason < kNameCount) {
    return kVanillaNames[reason];
  }
  if (reason == static_cast<std::uint32_t>(SpellCastResult::kNeedMoreItems)) {
    return "SPELL_FAILED_NEED_MORE_ITEMS";
  }
  return "SPELL_FAILED_UNKNOWN";
}

const char* PowerTypeToString(std::uint32_t power_type) {
  static const char* const kNames[] = {
    "MANA",
    "RAGE",
    "FOCUS",
    "ENERGY",
    "HAPPINESS",
    "RUNES",
    "RUNIC_POWER",
  };
  if (power_type < 7) return kNames[power_type];
  return "";
}

const char* PetTameFailureToString(std::uint8_t code) {
  switch (code) {
    case 1:  return "PETTAME_INVALIDCREATURE";
    case 2:  return "PETTAME_TOOMANY";
    case 3:  return "PETTAME_CREATUREALREADYOWNED";
    case 4:  return "PETTAME_NOTTAMEABLE";
    case 5:  return "PETTAME_ANOTHERSUMMONACTIVE";
    case 6:  return "PETTAME_UNITSCANTTAME";
    case 7:  return "PETTAME_NOPETAVAILABLE";
    case 8:  return "PETTAME_INTERNALERROR";
    case 9:  return "PETTAME_TOOHIGHLEVEL";
    case 10: return "PETTAME_DEAD";
    case 11: return "PETTAME_NOTDEAD";
    case 12: return "PETTAME_CANTCONTROLEXOTIC";
    default: return "PETTAME_UNKNOWNERROR";
  }
}

void DisplaySpellFailedNeedMoreItems(const ItemDefinitions& item_definitions,
                                     const std::uint32_t item_id,
                                     const std::uint32_t quantity) {
  const std::string format = Localization::Get().GetString(
      "SPELL_FAILED_NEED_MORE_ITEMS", "SPELL_FAILED_NEED_MORE_ITEMS");
  const ItemTemplate* const item = item_definitions.GetItem(item_id);

  char message[1024]{};
  if (item != nullptr) {
    core::FormatLocalized(message, sizeof(message), format.c_str(), quantity,
                          item->name.c_str());
  } else {

    core::FormatLocalized(message, sizeof(message), format.c_str(), 0u,
                          "UNKNOWN");
  }
  openwow::ui::game::DisplaySystemMessage(48, message);
}

void DisplaySpellFailedCustomError(const std::uint32_t custom_error_id) {
  char key[64]{};
  std::snprintf(key, sizeof(key), "SPELL_FAILED_CUSTOM_ERROR_%u",
                custom_error_id);
  const std::string message = Localization::Get().GetString(key, key);
  openwow::ui::game::DisplaySystemMessage(48, message.c_str());
}

void DisplayPetTameFailure(const std::uint8_t code) {
  const char* const global_string_key = PetTameFailureToString(code);
  const std::string localized_reason =
      Localization::Get().GetString(global_string_key, global_string_key);
  openwow::ui::game::DisplaySystemMessage(253, localized_reason.c_str());
}

void SpellCastFailure_OnItemTemplateReady(const ItemDefinitions& item_definitions,
                                          std::uint32_t item_entry,
                                          std::uint32_t ,
                                          std::uint32_t spell_cast_result) {

  const char* const key = SpellFailedReasonToString(spell_cast_result);
  const std::string format =
      Localization::Get().GetString(key, key);

  int system_msg_index = 48;
  if (spell_cast_result == 100) {
    system_msg_index = 225;
  } else if (spell_cast_result == 131) {
    system_msg_index = 224;
  }

  const char* item_name = "UNKNOWN";
  const ItemTemplate* tmpl = item_definitions.GetItem(item_entry);
  if (tmpl) {
    item_name = tmpl->name.c_str();
  }

  char buf[1024];
  core::FormatLocalized(buf, sizeof(buf), format.c_str(), item_name);
  openwow::ui::game::DisplaySystemMessage(system_msg_index, buf);
}

}
