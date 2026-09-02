#include "openwow/game/actions/adapters/protocol/wotlk_action_packets.h"

#include "openwow/network/protocol/wotlk/opcodes.h"

#include <algorithm>

namespace openwow::game::actions::adapters::protocol {

namespace {
constexpr std::size_t kClassicActionSlotCount = 120;
}

std::optional<DecodedActionAssignments> DecodeActionAssignments(
    const net::wotlk::WorldPacket& packet,
    const ActionAssignments::Storage* /*previous*/) {
  if (packet.opcode != net::wotlk::Opcode::SMSG_ACTION_BUTTONS ||
      packet.payload.empty() ||
      packet.payload.size() % sizeof(std::uint32_t) != 0) {
    return std::nullopt;
  }

  // Classic/Turtle sends a complete snapshot of packed action values. There
  // is no WotLK state byte before slot zero.
  ActionAssignments::Storage values{};
  const auto available_slots =
      std::min<std::size_t>(kClassicActionSlotCount,
                            packet.payload.size() / sizeof(std::uint32_t));
  for (std::size_t index = 0; index < available_slots; ++index) {
    const auto offset = index * sizeof(std::uint32_t);
    const auto packed =
        static_cast<std::uint32_t>(packet.payload[offset]) |
        (static_cast<std::uint32_t>(packet.payload[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(packet.payload[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(packet.payload[offset + 3]) << 24);
    values[index] = Action::Decode(packed);
  }
  return DecodedActionAssignments{ActionAssignmentSyncState::kUpdate, values};
}

net::wotlk::WorldPacket EncodeActionAssignment(ActionSlot slot,
                                                const Action& action) {
  net::wotlk::WorldPacket packet(
      net::wotlk::Opcode::CMSG_SET_ACTION_BUTTON);
  packet.AppendU8(slot.wire_value());
  packet.AppendU32(action.Encode());
  return packet;
}

}
