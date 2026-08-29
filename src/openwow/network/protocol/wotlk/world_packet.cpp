#include "openwow/network/protocol/wotlk/world_packet.h"

#include <bit>

namespace openwow::net::wotlk {
namespace {

// OllieWoW's 1.12 server uses asymmetric world headers. Client packets use
// uint16 size + uint32 opcode; server packets use uint16 size + uint16 opcode.
constexpr std::size_t kClientOpcodeSize = sizeof(std::uint32_t);
constexpr std::size_t kServerOpcodeSize = sizeof(std::uint16_t);

void AppendBigEndianSize(std::vector<std::uint8_t>& output,
                         std::size_t body_size) {
  // MaNGOS 1.12 uses a fixed uint16 size in network byte order.
  const auto encoded_size = static_cast<std::uint16_t>(body_size);
  output.push_back(static_cast<std::uint8_t>((encoded_size >> 8) & 0xFF));
  output.push_back(static_cast<std::uint8_t>(encoded_size & 0xFF));
}

}

std::size_t WorldPacket::ClientSizeFieldLength(std::size_t payload_size) {
  (void)payload_size;
  return sizeof(std::uint16_t);
}

std::vector<std::uint8_t> WorldPacket::SerializeClientFrame() const {
  const std::size_t body_size = kClientOpcodeSize + payload.size();
  std::vector<std::uint8_t> output;
  output.reserve(ClientSizeFieldLength(payload.size()) + body_size);

  AppendBigEndianSize(output, body_size);

  const auto opcode_value = static_cast<std::uint32_t>(OpcodeValue(opcode));
  output.push_back(static_cast<std::uint8_t>(opcode_value & 0xFF));
  output.push_back(static_cast<std::uint8_t>((opcode_value >> 8) & 0xFF));
  output.push_back(static_cast<std::uint8_t>((opcode_value >> 16) & 0xFF));
  output.push_back(static_cast<std::uint8_t>((opcode_value >> 24) & 0xFF));
  output.insert(output.end(), payload.begin(), payload.end());
  return output;
}

std::size_t WorldPacket::ServerSizeFieldLength(std::uint8_t first_byte) {
  (void)first_byte;
  return sizeof(std::uint16_t);
}

ServerFrameDecodeResult WorldPacket::DecodeServerFrame(
    std::span<const std::uint8_t> bytes) {
  if (bytes.size() < 2) {
    return {};
  }

  const std::size_t size_field_length = ServerSizeFieldLength(bytes.front());
  if (bytes.size() < size_field_length) {
    return {};
  }

  std::size_t body_size = 0;
  body_size = (static_cast<std::size_t>(bytes[0]) << 8) |
              static_cast<std::size_t>(bytes[1]);

  if (body_size < kServerOpcodeSize) {
    return {.status = ServerFrameDecodeStatus::kInvalidBodySize};
  }

  const std::size_t frame_size = size_field_length + body_size;
  if (bytes.size() < frame_size) {
    return {};
  }

  const auto opcode_value =
      static_cast<std::uint16_t>(bytes[size_field_length]) |
      (static_cast<std::uint16_t>(bytes[size_field_length + 1]) << 8);
  const std::size_t payload_offset = size_field_length + kServerOpcodeSize;

  ServerFrameDecodeResult result;
  result.status = ServerFrameDecodeStatus::kComplete;
  result.bytes_consumed = frame_size;
  result.packet.opcode = static_cast<Opcode>(opcode_value);
  result.packet.payload.assign(bytes.begin() + payload_offset,
                               bytes.begin() + frame_size);
  return result;
}

void WorldPacket::AppendU8(std::uint8_t value) {
  payload.push_back(value);
}

void WorldPacket::AppendU16(std::uint16_t value) {
  payload.push_back(static_cast<std::uint8_t>(value & 0xFF));
  payload.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void WorldPacket::AppendU32(std::uint32_t value) {
  payload.push_back(static_cast<std::uint8_t>(value & 0xFF));
  payload.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  payload.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  payload.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void WorldPacket::AppendU64(std::uint64_t value) {
  for (int byte_index = 0; byte_index < 8; ++byte_index) {
    payload.push_back(
        static_cast<std::uint8_t>((value >> (8 * byte_index)) & 0xFF));
  }
}

void WorldPacket::AppendFloat(float value) {
  AppendU32(std::bit_cast<std::uint32_t>(value));
}

void WorldPacket::AppendString(std::string_view value) {
  payload.insert(payload.end(), value.begin(), value.end());
  payload.push_back(0);
}

void WorldPacket::AppendBytes(const std::uint8_t* data, std::size_t size) {
  payload.insert(payload.end(), data, data + size);
}

}
