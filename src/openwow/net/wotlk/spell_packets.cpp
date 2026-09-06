
#include "openwow/net/wotlk/spell_packets.h"
#include "openwow/network/serialization/packed_guid_codec.h"

#include <cstring>

namespace openwow::net::wotlk {

namespace {

std::size_t ReadU8(const std::uint8_t* data, std::size_t len,
                   std::size_t offset, std::uint8_t& out) {
  if (offset + 1 > len) return 0;
  out = data[offset];
  return offset + 1;
}

std::size_t ReadU32(const std::uint8_t* data, std::size_t len,
                    std::size_t offset, std::uint32_t& out) {
  if (offset + 4 > len) return 0;
  std::memcpy(&out, data + offset, 4);
  return offset + 4;
}

std::size_t ReadFloat(const std::uint8_t* data, std::size_t len,
                      std::size_t offset, float& out) {
  if (offset + 4 > len) return 0;
  std::memcpy(&out, data + offset, 4);
  return offset + 4;
}

std::size_t ReadFullGuid(const std::uint8_t* data, std::size_t len,
                         std::size_t offset, game::ObjectGuid& out) {
  if (offset + 8 > len) return 0;
  std::uint64_t raw = 0;
  std::memcpy(&raw, data + offset, 8);
  out = game::ObjectGuid(raw);
  return offset + 8;
}

std::size_t ReadSpellTargetString(const std::uint8_t* data,
                                  std::size_t len, std::size_t offset,
                                  std::string& out) {
  out.clear();
  if (offset >= len) return 0;

  const auto* const begin = data + offset;
  const auto* const nul = static_cast<const std::uint8_t*>(
      std::memchr(begin, 0, len - offset));
  if (nul == nullptr) return 0;

  out.assign(reinterpret_cast<const char*>(begin),
             static_cast<std::size_t>(nul - begin));
  return static_cast<std::size_t>(nul - data) + 1;
}

std::size_t ReadU16(const std::uint8_t* data, std::size_t len,
                    std::size_t offset, std::uint16_t& out) {
  if (offset + 2 > len) return 0;
  std::memcpy(&out, data + offset, 2);
  return offset + 2;
}

std::size_t ReadSpellCastHeader(const std::uint8_t* data, const std::size_t len,
                                std::size_t offset, auto& result) {
  // Vanilla/Turtle SMSG_SPELL_START and SMSG_SPELL_GO share this header:
  // packed item-or-caster guid, packed caster guid, spell id, uint16 flags.
  offset = ReadPackedGuid(data, len, offset, result.caster_guid);
  if (offset == 0) return 0;
  offset = ReadPackedGuid(data, len, offset, result.caster_unit_guid);
  if (offset == 0) return 0;
  offset = ReadU32(data, len, offset, result.spell_id);
  if (offset == 0) return 0;

  std::uint16_t flags = 0;
  offset = ReadU16(data, len, offset, flags);
  result.cast_flags = static_cast<SpellCastFlags>(flags);
  result.cast_count = 0;
  return offset;
}

std::size_t ReadSpellPosition(const std::uint8_t* data, std::size_t len,
                              std::size_t offset, SpellPosition& out) {
  out.transport_guid = game::ObjectGuid{};
  offset = ReadFloat(data, len, offset, out.x);
  if (offset == 0) return 0;
  offset = ReadFloat(data, len, offset, out.y);
  if (offset == 0) return 0;
  offset = ReadFloat(data, len, offset, out.z);
  return offset;
}

std::size_t ReadSpellCastTargetsVanilla(const std::uint8_t* data,
                                        std::size_t len, std::size_t offset,
                                        SpellCastTargets& out) {
  out = {};

  std::uint16_t mask_raw = 0;
  offset = ReadU16(data, len, offset, mask_raw);
  if (offset == 0) return 0;
  out.target_mask = static_cast<SpellCastTargetFlags>(mask_raw);

  // SpellCastTargets::write() emits one packed guid for the first matching
  // target family, then the optional item guid, locations and C string.
  constexpr auto kObjectGuidMask =
      SpellCastTargetFlags::kUnit | SpellCastTargetFlags::kCorpseAlly |
      SpellCastTargetFlags::kGameObject | SpellCastTargetFlags::kCorpseEnemy |
      SpellCastTargetFlags::kUnitMinipet;
  if (HasFlag(out.target_mask, kObjectGuidMask)) {
    offset = ReadPackedGuid(data, len, offset, out.object_target_guid);
    if (offset == 0) return 0;
  }

  constexpr auto kItemGuidMask =
      SpellCastTargetFlags::kItem | SpellCastTargetFlags::kTradeItem;
  if (HasFlag(out.target_mask, kItemGuidMask)) {
    offset = ReadPackedGuid(data, len, offset, out.item_target_guid);
    if (offset == 0) return 0;
  }

  if (HasFlag(out.target_mask, SpellCastTargetFlags::kSourceLocation)) {
    SpellPosition pos;
    offset = ReadSpellPosition(data, len, offset, pos);
    if (offset == 0) return 0;
    out.source_location = pos;
  }

  if (HasFlag(out.target_mask, SpellCastTargetFlags::kDestLocation)) {
    SpellPosition pos;
    offset = ReadSpellPosition(data, len, offset, pos);
    if (offset == 0) return 0;
    out.dest_location = pos;
  }

  if (HasFlag(out.target_mask, SpellCastTargetFlags::kString)) {
    offset = ReadSpellTargetString(data, len, offset, out.target_string);
    if (offset == 0) return 0;
  }

  return offset;
}

std::size_t ReadOptionalAmmo(const std::uint8_t* data, const std::size_t len,
                             std::size_t offset, const SpellCastFlags flags,
                             std::optional<AmmoData>& ammo) {
  if (!HasFlag(flags, SpellCastFlags::kProjectile)) return offset;

  AmmoData value;
  offset = ReadU32(data, len, offset, value.display_id);
  if (offset == 0) return 0;
  offset = ReadU32(data, len, offset, value.inventory_type);
  if (offset != 0) ammo = value;
  return offset;
}

}

std::size_t ReadPackedGuid(const std::uint8_t* data, std::size_t len,
                           std::size_t offset, game::ObjectGuid& out) {
  if (!data || offset >= len) return 0;
  const auto decoded = openwow::net::DecodePackedGuid(
      data + offset, len - offset);
  if (!decoded) return 0;
  out = game::ObjectGuid(decoded.value);
  return offset + decoded.bytes_consumed;
}

std::size_t ReadSpellCastTargets(const std::uint8_t* data, std::size_t len,
                                 std::size_t offset, SpellCastTargets& out) {
  return ReadSpellCastTargetsVanilla(data, len, offset, out);
}

std::optional<SpellStartData>
ParseSpellStart(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return std::nullopt;

  SpellStartData result;
  std::size_t off = 0;

  off = ReadSpellCastHeader(data, len, off, result);
  if (off == 0) return std::nullopt;

  std::uint32_t cast_time = 0;
  off = ReadU32(data, len, off, cast_time);
  if (off == 0) return std::nullopt;
  result.cast_time = static_cast<std::int32_t>(cast_time);

  off = ReadSpellCastTargets(data, len, off, result.targets);
  if (off == 0) return std::nullopt;

  off = ReadOptionalAmmo(data, len, off, result.cast_flags, result.ammo);
  if (off == 0) return std::nullopt;

  return result;
}

std::optional<SpellGoData>
ParseSpellGo(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return std::nullopt;

  SpellGoData result;
  std::size_t off = 0;

  off = ReadSpellCastHeader(data, len, off, result);
  if (off == 0) return std::nullopt;

  std::uint8_t hit_count = 0;
  off = ReadU8(data, len, off, hit_count);
  if (off == 0) return std::nullopt;
  result.hit_targets.reserve(hit_count);
  for (std::uint8_t i = 0; i < hit_count; ++i) {
    game::ObjectGuid guid;
    off = ReadFullGuid(data, len, off, guid);
    if (off == 0) return std::nullopt;
    result.hit_targets.push_back(guid);
  }

  std::uint8_t miss_count = 0;
  off = ReadU8(data, len, off, miss_count);
  if (off == 0) return std::nullopt;
  result.miss_targets.reserve(miss_count);
  for (std::uint8_t i = 0; i < miss_count; ++i) {
    SpellMissEntry entry;
    off = ReadFullGuid(data, len, off, entry.target);
    if (off == 0) return std::nullopt;
    std::uint8_t reason = 0;
    off = ReadU8(data, len, off, reason);
    if (off == 0) return std::nullopt;
    entry.reason = static_cast<SpellMissInfo>(reason);
    if (entry.reason == SpellMissInfo::kReflect) {
      std::uint8_t reflect = 0;
      off = ReadU8(data, len, off, reflect);
      if (off == 0) return std::nullopt;
      entry.reflect = static_cast<SpellMissInfo>(reflect);
    }
    result.miss_targets.push_back(entry);
  }

  off = ReadSpellCastTargets(data, len, off, result.targets);
  if (off == 0) return std::nullopt;

  off = ReadOptionalAmmo(data, len, off, result.cast_flags, result.ammo);
  if (off == 0) return std::nullopt;

  return result;
}

std::optional<SpellFailureData>
ParseSpellFailure(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return std::nullopt;

  SpellFailureData result;
  std::size_t off = 0;

  off = ReadPackedGuid(data, len, off, result.caster_guid);
  if (off == 0) return std::nullopt;
  off = ReadU8(data, len, off, result.cast_count);
  if (off == 0) return std::nullopt;
  off = ReadU32(data, len, off, result.spell_id);
  if (off == 0) return std::nullopt;
  off = ReadU8(data, len, off, result.result);
  if (off == 0) return std::nullopt;

  return result;
}

std::optional<CastFailedData>
ParseCastFailed(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return std::nullopt;

  CastFailedData result;
  std::size_t off = 0;

  off = ReadU8(data, len, off, result.cast_count);
  if (off == 0) return std::nullopt;
  off = ReadU32(data, len, off, result.spell_id);
  if (off == 0) return std::nullopt;
  off = ReadU8(data, len, off, result.result);
  if (off == 0) return std::nullopt;

  while (off + 4 <= len) {
    std::uint32_t extra_val = 0;
    off = ReadU32(data, len, off, extra_val);
    if (off == 0) break;
    result.extra.push_back(extra_val);
  }

  return result;
}

std::optional<SpellDelayedData>
ParseSpellDelayed(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return std::nullopt;

  SpellDelayedData result;
  std::size_t off = 0;

  off = ReadPackedGuid(data, len, off, result.caster_guid);
  if (off == 0) return std::nullopt;
  off = ReadU32(data, len, off, result.delay_time);
  if (off == 0) return std::nullopt;

  return result;
}

std::optional<ChannelStartData>
ParseChannelStart(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return std::nullopt;

  ChannelStartData result;
  std::size_t off = 0;

  off = ReadPackedGuid(data, len, off, result.caster_guid);
  if (off == 0) return std::nullopt;
  off = ReadU32(data, len, off, result.spell_id);
  if (off == 0) return std::nullopt;
  off = ReadU32(data, len, off, result.duration);
  if (off == 0) return std::nullopt;

  return result;
}

std::optional<ChannelUpdateData>
ParseChannelUpdate(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) return std::nullopt;

  ChannelUpdateData result;
  std::size_t off = 0;

  off = ReadPackedGuid(data, len, off, result.caster_guid);
  if (off == 0) return std::nullopt;
  off = ReadU32(data, len, off, result.remaining);
  if (off == 0) return std::nullopt;

  return result;
}

}
