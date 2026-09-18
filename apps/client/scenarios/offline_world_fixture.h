#pragma once

#include "openwow/game/object_guid.h"
#include "openwow/network/protocol/wotlk/world_packet.h"

#include <cstdint>
#include <vector>

namespace openwow::game {
class WorldSession;
}

namespace openwow::client::offline_world_fixture {

// Eén aura zoals de 1.12-server hem in het unit-descriptor zet.
struct PlayerAuraFixture {
  std::uint8_t descriptor_slot{0};
  std::uint32_t spell_id{0};
  // 1.12-vlaggen: 0x01 cancelable, 0x04 onbekend3, 0x08 negatief (AFLAG_UNK4).
  std::uint8_t flags{0};
  std::uint8_t stacks{0};
};

// Values-update die de aura-velden van een bestaande unit zet: 48 spell-id's,
// de flags (8 auras per woord, 4 bits elk) en de applications (4 per woord,
// 1 byte elk). Vorm overgenomen van Source SpellAuras.cpp:7546-7575.
[[nodiscard]] openwow::net::wotlk::WorldPacket BuildPlayerAuras(
    const openwow::game::ObjectGuid player_guid,
    const std::vector<PlayerAuraFixture>& auras);

// 1.12 SMSG_UPDATE_AURA_DURATION (0x137): u8 descriptor-slot + u32 resterende
// duur in ms (Source SpellAuras.cpp:7596-7607).
[[nodiscard]] openwow::net::wotlk::WorldPacket BuildUpdateAuraDuration(
    std::uint8_t descriptor_slot, std::uint32_t remaining_ms);

// Sessie-niveau controle van de hele aura-keten: voedt de packets hierboven en
// assert via dezelfde query-route als de Lua (AuraLuaBridge) dat de auras en de
// duur er staan. Logt bij falen de reden en geeft false terug.
[[nodiscard]] bool ValidateOfflineAuraFixtures(
    openwow::game::WorldSession& session,
    const openwow::game::ObjectGuid player_guid);

// Pure functietest van de 1.12-wereldcursor-ladder (unit_cursor_policy.h):
// NPC-flags -> cursor, de grijs-offset en de 5.5556-yd-grens. Geen sessie nodig.
[[nodiscard]] bool ValidateUnitCursorPolicyFixtures();

[[nodiscard]] openwow::net::wotlk::WorldPacket BuildInitialSpells();

[[nodiscard]] openwow::net::wotlk::WorldPacket BuildActionAssignments();

[[nodiscard]] openwow::net::wotlk::WorldPacket BuildVisibleCreatureCreates(
    float x, float y, float z, float orientation);

[[nodiscard]] bool ValidateClassicUpdateObjectFixtures();

}
