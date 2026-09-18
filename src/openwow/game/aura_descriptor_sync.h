#pragma once

#include "openwow/game/aura_manager.h"

#include <cstdint>
#include <vector>

namespace openwow::game {

class WorldSession;
class CGObject_C;

// Bouwt de descriptor-auras van een unit opnieuw op in AuraTracker EN
// AuraManager. Nodig omdat 1.12 de aura-lijst in het create-blok stuurt en de
// descriptor-registry daar geen section-callbacks dispatcht
// (descriptor_callback_registry.cpp:373): zonder deze aanroep blijft de tracker
// leeg tot de eerste aura-wijziging en toont de buffbalk niets.
void SyncAurasFromUnitDescriptor(const WorldSession& session,
                                 const ObjectGuid& guid);

// Zelfde, maar met het object erbij. Het create-pad heeft het object al en
// hoeft het dan niet via de objectmanager op te zoeken.
void SyncAurasFromUnitDescriptor(const WorldSession& session,
                                 const CGObject_C& object,
                                 const ObjectGuid& guid);

// 1.12 stuurt de aura-lijst van een unit in het unit-descriptor (niet als los
// pakket) en de resterende duur per aura via SMSG_UPDATE_AURA_DURATION (0x137).
// Onze AuraManager is op de 3.3.5-pakketlezer gebouwd (HandleAuraUpdate) en blijft
// daardoor leeg. Deze functies vullen dezelfde store vanuit het descriptor, zodat
// GetAuras en FindAuraBySpellId ook in 1.12 werken -- daar hangen CancelUnitBuff,
// de pet-/pvp-/quest-Lua en pet_session aan.
void SetDescriptorAurasForUnit(std::uint64_t guid,
                               std::vector<AuraSlotInfo> auras);
void SetDescriptorAuraDuration(std::uint64_t guid,
                               std::uint8_t descriptor_slot,
                               std::uint32_t remaining_ms);
void ClearDescriptorAurasForUnit(std::uint64_t guid);

// Duur die binnenkwam voordat de aura bekend was (SMSG_UPDATE_AURA_DURATION
// arriveert in de praktijk vóór de object-update met het aura-veld). Geeft de
// waarde één keer terug en wist hem; 0 als er niets openstaat.
[[nodiscard]] std::uint32_t TakeDescriptorAuraDuration(
    std::uint64_t guid, std::uint8_t descriptor_slot);

// Duur per descriptor-slot voor de lokale speler, onafhankelijk van de guid.
// Nodig omdat de server bij het toetreden tot de map de duur van alle bestaande
// auras stuurt (Source Objects/Map.cpp:548-550) en die burst de client kan
// bereiken voordat het spelerobject bestaat; de echte 1.12-client bewaart de
// duur daarom slot-geindexeerd. 0 = onbekend/wissen.
void SetLocalPlayerAuraDuration(std::uint8_t descriptor_slot,
                                 std::uint32_t remaining_ms);
[[nodiscard]] std::uint32_t GetLocalPlayerAuraDuration(
    std::uint8_t descriptor_slot);

}