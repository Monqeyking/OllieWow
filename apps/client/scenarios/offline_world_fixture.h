#pragma once

#include "openwow/network/protocol/wotlk/world_packet.h"

namespace openwow::client::offline_world_fixture {

[[nodiscard]] openwow::net::wotlk::WorldPacket BuildInitialSpells();

[[nodiscard]] openwow::net::wotlk::WorldPacket BuildActionAssignments();

[[nodiscard]] openwow::net::wotlk::WorldPacket BuildVisibleCreatureCreates(
    float x, float y, float z, float orientation);

[[nodiscard]] bool ValidateClassicUpdateObjectFixtures();

}
