
#pragma once

#include <cstdint>

namespace openwow::game {

namespace MirrorSection {
inline constexpr std::uint8_t kUnit   = 3;
inline constexpr std::uint8_t kPlayer = 4;
}

inline constexpr std::uint32_t kEventInventoryChanged  = 147;
inline constexpr std::uint32_t kEventQuestLogChanged   = 460;

// Byte-offsets vanaf UNIT_END, dus (veld - UNIT_END) * 4.
//
// Hier stonden 3.3.5-waarden. PLAYER_FIELD_INV_SLOT_HEAD is in 1.12
// UNIT_END + 0x12A (Source Objects/UpdateFields.h:202, en Benilla
// assert_eq!(FIELD_PLAYER_INV_SLOT_HEAD, UNIT_END + 0x12A) in
// benilla-protocol/src/messages/update_object/fields/tests.rs:14, plus het
// getal 486 in benilla-app/src/ui_items/mod.rs:1265). Als byte-offset is dat
// 0x12A * 4 = 0x4A8. De oude 0x21C is de WotLK-woordoffset en werd hier als
// bytes gebruikt, dus UNIT_INVENTORY_CHANGED stond over een verkeerd bereik.
inline constexpr std::uint32_t kPlayerInventoryOffset = 0x4A8;
inline constexpr std::uint32_t kPlayerInventorySize   = 152;

// PLAYER_QUEST_LOG_1_1 is UNIT_END + 0x0A, dus 0x28 bytes -- dat klopte al.
// Maar 1.12 heeft 20 quests van 3 velden (Source UpdateFields.h:186 "20x3", en
// ons eigen update_fields.h met PLAYER_QUEST_LOG_LAST_1 = UNIT_END + 0x43),
// geen 25 van 5. Met de WotLK-stride belandden slots 1..19 op verkeerde velden
// en slots 20..24 voorbij het blok, waardoor UNIT_QUEST_LOG_CHANGED op de
// verkeerde plekken vuurde en het questlog niet bijwerkte.
inline constexpr std::uint32_t kPlayerQuestLogOffset       = 0x28;
inline constexpr std::uint32_t kPlayerQuestLogSlotCount    = 20;
inline constexpr std::uint32_t kPlayerQuestLogSlotSize     = 3 * sizeof(std::uint32_t);

int PlayerInventoryChangedCallback(std::uint32_t guid_low,
                                   std::uint32_t guid_high);

int PlayerQuestLogChangedCallback(std::uint32_t guid_low,
                                  std::uint32_t guid_high);

void Player_RegisterUnitFieldEventCallbacks(void* player_obj);

void Player_UnregisterUnitFieldEventCallbacks(void* player_obj);

}
