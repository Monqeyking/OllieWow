#pragma once

// Contextgevoelige wereldcursor (1.12). Het contract komt uit de client-classifier
// (`CGWorldFrame` 0x4828d0 -> unit-tak 0x482200) zoals byte-verified beschreven in
// Benilla `target/cursor_mode.rs:1-30,145-182,647-682`:
//   - de service-ladder over UNIT_NPC_FLAGS, laagste bit wint;
//   - NPC-diensten grijzen voorbij 5.5556 yd (gekwadrateerd 30.864, grens-inclusief);
//   - de QUESTGIVER-tak is gegate op de laatste SMSG_QUESTGIVER_STATUS (>= 2 betekent
//     "heeft een quest"; NONE/UNAVAILABLE niet).
// De cursor-BLP's zijn `Interface\Cursor\<Naam>.blp`.
//
// Header-only en puur (flags + afstand in, cursor-index uit) zodat de offline
// scenario-fixture dezelfde functies kan asserten als de game-loop gebruikt.

#include <cstdint>

namespace openwow::game {

// 3.3.5-tabel-indexen die in 1.12 bestaan (zie de stems-tabel in cursor_surface.cpp).
constexpr std::uint32_t kUnavailableCursorTypeOffset = 26u;
constexpr std::uint32_t kCursorTypeBuy = 3u;
constexpr std::uint32_t kCursorTypeAttack = 4u;
constexpr std::uint32_t kCursorTypeInteract = 5u;
constexpr std::uint32_t kCursorTypeSpeak = 6u;
constexpr std::uint32_t kCursorTypePickup = 8u;
constexpr std::uint32_t kCursorTypeTaxi = 9u;
constexpr std::uint32_t kCursorTypeTrainer = 10u;
constexpr std::uint32_t kCursorTypeMine = 11u;
constexpr std::uint32_t kCursorTypeSkin = 12u;
constexpr std::uint32_t kCursorTypeGatherHerbs = 13u;
constexpr std::uint32_t kCursorTypeLootAll = 16u;
constexpr std::uint32_t kCursorTypeRepairNpc = 18u;
constexpr std::uint32_t kCursorTypeSkinHorde = 20u;
constexpr std::uint32_t kCursorTypeSkinAlliance = 21u;
constexpr std::uint32_t kCursorTypeInnkeeper = 22u;
constexpr std::uint32_t kCursorTypeVehicle = 26u;

// 1.12/Classic-nummering; zie Source\src\game\Objects\UnitDefines.h.
constexpr std::uint32_t kCursorNpcFlagGossip = 0x00000001u;
constexpr std::uint32_t kCursorNpcFlagQuestGiver = 0x00000002u;
constexpr std::uint32_t kCursorNpcFlagVendor = 0x00000004u;
constexpr std::uint32_t kCursorNpcFlagFlightMaster = 0x00000008u;
constexpr std::uint32_t kCursorNpcFlagTrainer = 0x00000010u;
constexpr std::uint32_t kCursorNpcFlagSpiritHealer = 0x00000020u;
constexpr std::uint32_t kCursorNpcFlagSpiritGuide = 0x00000040u;
constexpr std::uint32_t kCursorNpcFlagInnkeeper = 0x00000080u;
constexpr std::uint32_t kCursorNpcFlagBanker = 0x00000100u;
constexpr std::uint32_t kCursorNpcFlagPetitioner = 0x00000200u;
constexpr std::uint32_t kCursorNpcFlagTabardDesigner = 0x00000400u;
constexpr std::uint32_t kCursorNpcFlagBattlemaster = 0x00000800u;
constexpr std::uint32_t kCursorNpcFlagAuctioneer = 0x00001000u;
constexpr std::uint32_t kCursorNpcFlagStableMaster = 0x00002000u;
constexpr std::uint32_t kCursorNpcFlagRepair = 0x00004000u;
// Bestaat niet in 1.12; de bits vuren daar nooit. Laten staan zodat de bedoeling
// leesbaar blijft.
constexpr std::uint32_t kCursorNpcFlagGuildBanker = 0x00800000u;
constexpr std::uint32_t kCursorNpcFlagSpellClick = 0x01000000u;

// NPC-diensten grijzen voorbij 5.5556 yd: de client vergelijkt met 30.864 (0x482320).
constexpr float kServiceRangeSquared = 30.864f;

[[nodiscard]] inline std::uint32_t ApplyUnavailableCursorOffset(
    const std::uint32_t base_type, const bool unavailable) {
  return unavailable ? base_type + kUnavailableCursorTypeOffset : base_type;
}

[[nodiscard]] inline bool NpcServiceCursorOutOfRange(const float distance_squared) {
  return distance_squared > kServiceRangeSquared;
}

// `wire_status` is de ruwe 1.12-waarde uit SMSG_QUESTGIVER_STATUS
// (Source QuestDef.h:121-130). Alles vanaf 2 (CHAT/INCOMPLETE/REWARD_REP/AVAILABLE/
// REWARD_OLD/REWARD2) betekent dat er iets te doen is.
[[nodiscard]] inline bool QuestGiverHasQuest(const std::uint32_t wire_status) {
  return wire_status >= 2u;
}

// De questgiver-tak: 1.12 gebruikt hier de gewone praat-cursor, niet de 3.3.5
// Quest/QuestTurnIn-cursors (die bestaan in 1.12 niet en leverden dus geen cursor op).
[[nodiscard]] inline std::uint32_t ResolveQuestGiverCursorType(
    const std::uint32_t wire_status, const bool out_of_range) {
  if (!QuestGiverHasQuest(wire_status)) {
    return 0u;
  }
  return ApplyUnavailableCursorOffset(kCursorTypeSpeak, out_of_range);
}

// De service-ladder: laagste bit wint. REPAIR wordt nooit geconsulteerd (een
// repair-only unit valt door naar de attack/clear-tak).
[[nodiscard]] inline std::uint32_t ResolveNpcServiceCursorType(
    const std::uint32_t npc_flags, const bool out_of_range) {
  const auto with_offset = [out_of_range](const std::uint32_t base_type) {
    return ApplyUnavailableCursorOffset(base_type, out_of_range);
  };

  if ((npc_flags & kCursorNpcFlagGossip) != 0u) {
    return with_offset(kCursorTypeSpeak);
  }
  if ((npc_flags & kCursorNpcFlagVendor) != 0u) {
    return with_offset(kCursorTypePickup);
  }
  if ((npc_flags & kCursorNpcFlagFlightMaster) != 0u) {
    return with_offset(kCursorTypeTaxi);
  }
  if ((npc_flags & kCursorNpcFlagTrainer) != 0u) {
    return with_offset(kCursorTypeTrainer);
  }
  if ((npc_flags & (kCursorNpcFlagSpiritHealer | kCursorNpcFlagSpiritGuide)) != 0u) {
    return with_offset(kCursorTypeSpeak);
  }
  if ((npc_flags & kCursorNpcFlagInnkeeper) != 0u) {
    return with_offset(kCursorTypeInteract);
  }
  if ((npc_flags & (kCursorNpcFlagBanker | kCursorNpcFlagGuildBanker)) != 0u) {
    return with_offset(kCursorTypeBuy);
  }
  if ((npc_flags & (kCursorNpcFlagPetitioner | kCursorNpcFlagTabardDesigner |
                    kCursorNpcFlagBattlemaster)) != 0u) {
    return with_offset(kCursorTypeSpeak);
  }
  if ((npc_flags & kCursorNpcFlagAuctioneer) != 0u) {
    return with_offset(kCursorTypeBuy);
  }
  if ((npc_flags & kCursorNpcFlagStableMaster) != 0u) {
    return with_offset(kCursorTypeSpeak);
  }
  if ((npc_flags & kCursorNpcFlagSpellClick) != 0u) {
    return with_offset(kCursorTypeInteract);
  }
  return 0u;
}

}  // namespace openwow::game