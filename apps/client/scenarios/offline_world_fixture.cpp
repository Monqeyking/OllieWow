#include "scenarios/offline_world_fixture.h"

#include "openwow/game/actions/model/action_assignments.h"
#include "openwow/game/object_guid.h"
#include "openwow/game/object_types.h"
#include "openwow/game/update_fields.h"
#include "openwow/game/update_object_parser.h"
#include "openwow/network/protocol/wotlk/opcodes.h"
#include "openwow/foundation/diagnostics/logging.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace openwow::client::offline_world_fixture {

namespace {

constexpr std::uint32_t kPrimaryActionSpell = 6603u;

void AppendU8(std::vector<std::uint8_t>& bytes, const std::uint8_t value) {
  bytes.push_back(value);
}

void AppendU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
  for (std::uint32_t shift = 0; shift < 32u; shift += 8u) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void AppendU64(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
  for (std::uint32_t shift = 0; shift < 64u; shift += 8u) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

void AppendFloat(std::vector<std::uint8_t>& bytes, const float value) {
  std::uint32_t bits = 0u;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  AppendU32(bytes, bits);
}

void AppendPackedGuid(std::vector<std::uint8_t>& bytes,
                      const openwow::game::ObjectGuid guid) {
  const std::uint64_t raw = guid.GetRawValue();
  std::uint8_t mask = 0u;
  std::vector<std::uint8_t> packed;
  packed.reserve(8u);
  for (std::size_t index = 0; index < 8u; ++index) {
    const auto byte = static_cast<std::uint8_t>(raw >> (index * 8u));
    if (byte == 0u) {
      continue;
    }
    mask |= static_cast<std::uint8_t>(1u << index);
    packed.push_back(byte);
  }
  AppendU8(bytes, mask);
  bytes.insert(bytes.end(), packed.begin(), packed.end());
}

void AppendFields(
    std::vector<std::uint8_t>& bytes, const std::uint16_t field_count,
    std::initializer_list<std::pair<std::uint16_t, std::uint32_t>> fields) {
  const auto block_count = openwow::game::BitmaskBlockCount(field_count);
  std::vector<std::uint32_t> masks(block_count, 0u);
  std::vector<std::pair<std::uint16_t, std::uint32_t>> ordered(fields);
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
  for (const auto& [index, value] : ordered) {
    (void)value;
    masks[index / 32u] |= 1u << (index % 32u);
  }
  AppendU8(bytes, block_count);
  for (const auto mask : masks) {
    AppendU32(bytes, mask);
  }
  for (const auto& [index, value] : ordered) {
    (void)index;
    AppendU32(bytes, value);
  }
}

std::vector<std::uint8_t> BuildClassicCreate(
    const openwow::game::TypeID type_id,
    const openwow::game::ObjectGuid guid, const float x = 1.0F,
    const float y = 2.0F, const float z = 3.0F,
    const float orientation = 0.5F,
    const std::uint32_t unit_display_id = 49u) {
  using namespace openwow::game;

  std::vector<std::uint8_t> bytes;
  AppendU32(bytes, 1u);
  AppendU8(bytes, 0u);
  AppendU8(bytes, type_id == TypeID::kPlayer
                     ? static_cast<std::uint8_t>(UpdateType::kCreateObject2)
                     : static_cast<std::uint8_t>(UpdateType::kCreateObject));
  AppendPackedGuid(bytes, guid);
  AppendU8(bytes, static_cast<std::uint8_t>(type_id));

  if (type_id == TypeID::kPlayer || type_id == TypeID::kUnit) {
    AppendU8(bytes, type_id == TypeID::kPlayer
                       ? kUpdateFlagLiving | kUpdateFlagSelf
                       : kUpdateFlagLiving);
    AppendU32(bytes, 0u);
    AppendU32(bytes, 0u);
    AppendFloat(bytes, x);
    AppendFloat(bytes, y);
    AppendFloat(bytes, z);
    AppendFloat(bytes, orientation);
    AppendU32(bytes, 0u);
    for (const float speed : {2.5F, 7.0F, 4.5F, 4.722222F, 2.5F, 7.0F}) {
      AppendFloat(bytes, speed);
    }
  } else {
    AppendU8(bytes, kUpdateFlagPosition | kUpdateFlagLowGuid);
    AppendFloat(bytes, 1.0F);
    AppendFloat(bytes, 2.0F);
    AppendFloat(bytes, 3.0F);
    AppendFloat(bytes, 0.5F);
    AppendU32(bytes, 1u);
  }

  const auto raw = guid.GetRawValue();
  switch (type_id) {
    case TypeID::kPlayer:
      AppendFields(bytes, PLAYER_END,
                   {{OBJECT_FIELD_GUID, static_cast<std::uint32_t>(raw)},
                    {OBJECT_FIELD_GUID + 1u, static_cast<std::uint32_t>(raw >> 32u)},
                    {OBJECT_FIELD_TYPE, TypeMaskFor(type_id)},
                    {UNIT_FIELD_HEALTH, 100u},
                    {PLAYER_FLAGS, 0u}});
      break;
    case TypeID::kUnit:
      AppendFields(bytes, UNIT_END,
                   {{OBJECT_FIELD_GUID, static_cast<std::uint32_t>(raw)},
                    {OBJECT_FIELD_GUID + 1u, static_cast<std::uint32_t>(raw >> 32u)},
                    {OBJECT_FIELD_TYPE, TypeMaskFor(type_id)},
                    {UNIT_FIELD_BYTES_0, 1u | (1u << 8u)},
                    {UNIT_FIELD_HEALTH, 100u},
                    {UNIT_FIELD_MAXHEALTH, 100u},
                    {UNIT_FIELD_LEVEL, 5u},
                    {UNIT_FIELD_FACTIONTEMPLATE, 1u},
                    {UNIT_FIELD_BOUNDINGRADIUS, 0x3E9BA5E3u},
                    {UNIT_FIELD_COMBATREACH, 0x3FC00000u},
                    {UNIT_FIELD_DISPLAYID, unit_display_id},
                    {UNIT_FIELD_NATIVEDISPLAYID, unit_display_id}});
      break;
    case TypeID::kItem:
      AppendFields(bytes, ITEM_END,
                   {{OBJECT_FIELD_GUID, static_cast<std::uint32_t>(raw)},
                    {OBJECT_FIELD_GUID + 1u, static_cast<std::uint32_t>(raw >> 32u)},
                    {OBJECT_FIELD_TYPE, TypeMaskFor(type_id)},
                    {ITEM_FIELD_STACK_COUNT, 1u}});
      break;
    case TypeID::kGameObject:
      AppendFields(bytes, GAMEOBJECT_END,
                   {{OBJECT_FIELD_GUID, static_cast<std::uint32_t>(raw)},
                    {OBJECT_FIELD_GUID + 1u, static_cast<std::uint32_t>(raw >> 32u)},
                    {OBJECT_FIELD_TYPE, TypeMaskFor(type_id)},
                    {GAMEOBJECT_DISPLAYID, 123u},
                    {GAMEOBJECT_STATE, 1u},
                    {GAMEOBJECT_TYPE_ID, 3u},
                    {GAMEOBJECT_ARTKIT, 2u},
                    {GAMEOBJECT_ANIMPROGRESS, 4u}});
      break;
    case TypeID::kCorpse:
      AppendFields(bytes, CORPSE_END,
                   {{OBJECT_FIELD_GUID, static_cast<std::uint32_t>(raw)},
                    {OBJECT_FIELD_GUID + 1u, static_cast<std::uint32_t>(raw >> 32u)},
                    {OBJECT_FIELD_TYPE, TypeMaskFor(type_id)},
                    {CORPSE_FIELD_DISPLAY_ID, 49u},
                    {CORPSE_FIELD_BYTES_1, 1u}});
      break;
    case TypeID::kDynamicObject:
      AppendFields(bytes, DYNAMICOBJECT_END,
                   {{OBJECT_FIELD_GUID, static_cast<std::uint32_t>(raw)},
                    {OBJECT_FIELD_GUID + 1u, static_cast<std::uint32_t>(raw >> 32u)},
                    {OBJECT_FIELD_TYPE, TypeMaskFor(type_id)},
                    {DYNAMICOBJECT_SPELLID, 1337u},
                    {DYNAMICOBJECT_RADIUS, 4u}});
      break;
    default:
      break;
  }
  return bytes;
}

std::vector<std::uint8_t> BuildClassicValues(const openwow::game::ObjectGuid guid) {
  using namespace openwow::game;
  std::vector<std::uint8_t> bytes;
  AppendU32(bytes, 1u);
  AppendU8(bytes, 0u);
  AppendU8(bytes, static_cast<std::uint8_t>(UpdateType::kValues));
  AppendPackedGuid(bytes, guid);
  AppendFields(bytes, PLAYER_END, {{UNIT_FIELD_HEALTH, 125u}});
  return bytes;
}

std::vector<std::uint8_t> BuildClassicMovement(const openwow::game::ObjectGuid guid) {
  using namespace openwow::game;
  std::vector<std::uint8_t> bytes;
  AppendU32(bytes, 1u);
  AppendU8(bytes, 0u);
  AppendU8(bytes, static_cast<std::uint8_t>(UpdateType::kMovement));
  AppendU64(bytes, guid.GetRawValue());
  AppendU8(bytes, kUpdateFlagLiving);
  AppendU32(bytes, 0u);
  AppendU32(bytes, 0u);
  AppendFloat(bytes, 1.0F);
  AppendFloat(bytes, 2.0F);
  AppendFloat(bytes, 3.0F);
  AppendFloat(bytes, 0.5F);
  AppendU32(bytes, 0u);
  for (const float speed : {2.5F, 7.0F, 4.5F, 4.722222F, 2.5F, 7.0F}) {
    AppendFloat(bytes, speed);
  }
  return bytes;
}

std::vector<std::uint8_t> BuildClassicTransportMovement(
    const openwow::game::ObjectGuid guid,
    const openwow::game::ObjectGuid transport_guid) {
  using namespace openwow::game;
  std::vector<std::uint8_t> bytes;
  AppendU32(bytes, 1u);
  AppendU8(bytes, 0u);
  AppendU8(bytes, static_cast<std::uint8_t>(UpdateType::kMovement));
  AppendU64(bytes, guid.GetRawValue());
  AppendU8(bytes, kUpdateFlagLiving);
  AppendU32(bytes, kMoveFlagOnTransport);
  AppendU32(bytes, 100u);
  AppendFloat(bytes, 1.0F);
  AppendFloat(bytes, 2.0F);
  AppendFloat(bytes, 3.0F);
  AppendFloat(bytes, 0.5F);
  AppendU64(bytes, transport_guid.GetRawValue());
  AppendFloat(bytes, 0.1F);
  AppendFloat(bytes, 0.2F);
  AppendFloat(bytes, 0.3F);
  AppendFloat(bytes, 0.4F);
  AppendU32(bytes, 0u);
  for (const float speed : {2.5F, 7.0F, 4.5F, 4.722222F, 2.5F, 7.0F}) {
    AppendFloat(bytes, speed);
  }
  return bytes;
}

std::vector<std::uint8_t> BuildClassicGuidList(
    const openwow::game::UpdateType type,
    const openwow::game::ObjectGuid guid) {
  std::vector<std::uint8_t> bytes;
  AppendU32(bytes, 1u);
  AppendU8(bytes, 0u);
  AppendU8(bytes, static_cast<std::uint8_t>(type));
  AppendU32(bytes, 1u);
  AppendPackedGuid(bytes, guid);
  return bytes;
}

bool ParseSuccessfully(const std::vector<std::uint8_t>& payload,
                       const openwow::game::UpdateObjectHandler& handler) {
  return openwow::game::ParseUpdateObject(payload.data(), payload.size(), handler);
}

}

openwow::net::wotlk::WorldPacket BuildInitialSpells() {
  openwow::net::wotlk::WorldPacket packet(
      openwow::net::wotlk::Opcode::SMSG_INITIAL_SPELLS);
  packet.AppendU8(0u);
  packet.AppendU16(1u);
  packet.AppendU32(kPrimaryActionSpell);
  packet.AppendU16(0u);
  packet.AppendU16(0u);
  return packet;
}

openwow::net::wotlk::WorldPacket BuildActionAssignments() {
  using openwow::game::actions::Action;
  using openwow::game::actions::ActionAssignmentSyncState;
  using openwow::game::actions::ActionKind;
  using openwow::game::actions::ActionSlot;

  openwow::net::wotlk::WorldPacket packet(
      openwow::net::wotlk::Opcode::SMSG_ACTION_BUTTONS);
  packet.payload.reserve(1u + ActionSlot::kCount * sizeof(std::uint32_t));
  packet.AppendU8(
      static_cast<std::uint8_t>(ActionAssignmentSyncState::kUpdate));

  constexpr auto primary_action =
      Action::Create(ActionKind::kSpell, kPrimaryActionSpell);
  static_assert(primary_action.has_value());
  packet.AppendU32(primary_action->Encode());
  for (std::size_t slot = 1; slot < ActionSlot::kCount; ++slot) {
    packet.AppendU32(0u);
  }
  return packet;
}

openwow::net::wotlk::WorldPacket BuildVisibleCreatureCreates(
    const float x, const float y, const float z, const float orientation) {
  using openwow::game::HighGuid;
  using openwow::game::ObjectGuid;
  using openwow::game::TypeID;

  const std::array<std::tuple<ObjectGuid, float, float, float>, 2> creatures{{
      {ObjectGuid::Create(HighGuid::kUnit, 299u, 1u), x + 1.5F, y + 4.0F, z},
      {ObjectGuid::Create(HighGuid::kUnit, 300u, 2u), x - 1.5F, y + 4.0F, z},
  }};

  std::vector<std::uint8_t> payload;
  AppendU32(payload, static_cast<std::uint32_t>(creatures.size()));
  AppendU8(payload, 0u);
  for (const auto& [guid, creature_x, creature_y, creature_z] : creatures) {
    const auto block = BuildClassicCreate(TypeID::kUnit, guid, creature_x,
                                          creature_y, creature_z, orientation,
                                          31u);
    payload.insert(payload.end(), block.begin() + 5, block.end());
  }

  openwow::net::wotlk::WorldPacket packet(
      openwow::net::wotlk::Opcode::SMSG_UPDATE_OBJECT);
  packet.payload = std::move(payload);
  return packet;
}

bool ValidateClassicUpdateObjectFixtures() {
  using namespace openwow::game;

  const std::array<std::pair<TypeID, ObjectGuid>, 6> create_fixtures{{
      {TypeID::kPlayer, ObjectGuid(0x0102030405060708ull)},
      {TypeID::kUnit, ObjectGuid(0x1102030405060708ull)},
      {TypeID::kItem, ObjectGuid(0x2102030405060708ull)},
      {TypeID::kGameObject, ObjectGuid(0x3102030405060708ull)},
      {TypeID::kCorpse, ObjectGuid(0x4102030405060708ull)},
      {TypeID::kDynamicObject, ObjectGuid(0x5102030405060708ull)},
  }};

  for (const auto& [expected_type, guid] : create_fixtures) {
    std::size_t create_count = 0u;
    const auto payload = BuildClassicCreate(expected_type, guid);
    UpdateObjectHandler handler;
    handler.on_create = [&](const CreateObjectUpdate& update) {
      if (update.guid != guid || update.type_id != expected_type ||
          update.fields.field_count != FieldCountFor(expected_type) ||
          update.fields.values.empty()) {
        create_count = 99u;
        return;
      }
      ++create_count;
    };
    UpdateObjectParseStats stats;
    if (!ParseUpdateObject(payload.data(), payload.size(), handler, &stats) ||
        create_count != 1u) {
      openwow::diagnostics::Log(
          openwow::diagnostics::LogLevel::kError,
          "Classic update-object fixture failed type=" +
              std::to_string(static_cast<unsigned>(expected_type)) +
              " bytes=" + std::to_string(payload.size()) +
              " completed=" + std::to_string(stats.completed_blocks));
      return false;
    }
  }

  const ObjectGuid player_guid(0x0102030405060708ull);
  {
    bool values_seen = false;
    UpdateObjectHandler handler;
    handler.resolve_values_field_count = [player_guid](const ObjectGuid guid)
        -> std::optional<std::uint16_t> {
      return guid == player_guid ? std::optional<std::uint16_t>(PLAYER_END)
                                  : std::nullopt;
    };
    handler.on_values = [&values_seen](const ValuesUpdate& update) {
      values_seen = update.fields.GetValue(UNIT_FIELD_HEALTH) == 125u;
    };
    if (!ParseSuccessfully(BuildClassicValues(player_guid), handler) || !values_seen) {
      return false;
    }
  }

  {
    bool movement_seen = false;
    UpdateObjectHandler handler;
    handler.on_movement = [&movement_seen, player_guid](const MovementOnlyUpdate& update) {
      movement_seen = update.guid == player_guid &&
                      update.movement.update_flags == kUpdateFlagLiving &&
                      update.movement.speeds[5] == 7.0F;
    };
    if (!ParseSuccessfully(BuildClassicMovement(player_guid), handler) || !movement_seen) {
      return false;
    }
  }

  {
    const ObjectGuid transport_guid(0x6102030405060708ull);
    bool transport_seen = false;
    UpdateObjectHandler handler;
    handler.on_movement = [&transport_seen, player_guid, transport_guid](
                              const MovementOnlyUpdate& update) {
      transport_seen = update.guid == player_guid &&
                       update.movement.movement.transport.guid == transport_guid &&
                       update.movement.speeds[5] == 7.0F;
    };
    if (!ParseSuccessfully(BuildClassicTransportMovement(player_guid, transport_guid),
                           handler) ||
        !transport_seen) {
      return false;
    }
  }

  {
    const ObjectGuid listed_guid(0x7102030405060708ull);
    bool out_of_range_seen = false;
    UpdateObjectHandler handler;
    handler.on_out_of_range = [&out_of_range_seen, listed_guid](
                                  const OutOfRangeUpdate& update) {
      out_of_range_seen = update.guids.size() == 1u && update.guids.front() == listed_guid;
    };
    if (!ParseSuccessfully(
            BuildClassicGuidList(UpdateType::kOutOfRangeObjects, listed_guid), handler) ||
        !out_of_range_seen) {
      return false;
    }

    bool near_seen = false;
    handler = {};
    handler.on_near_objects = [&near_seen, listed_guid](const NearObjectsUpdate& update) {
      near_seen = update.guids.size() == 1u && update.guids.front() == listed_guid;
    };
    if (!ParseSuccessfully(BuildClassicGuidList(UpdateType::kNearObjects, listed_guid),
                           handler) ||
        !near_seen) {
      return false;
    }
  }

  auto truncated = BuildClassicCreate(TypeID::kPlayer, player_guid);
  truncated.pop_back();
  if (ParseSuccessfully(truncated, UpdateObjectHandler{})) {
    return false;
  }

  const std::array<std::uint8_t, 5> invalid_mask{
      1u, 0u, 0u, 0u, 0x80u};
  PacketReader invalid_mask_reader(invalid_mask.data(), invalid_mask.size());
  UpdateFieldValues invalid_mask_fields;
  if (ReadUpdateFields(invalid_mask_reader, 1u, invalid_mask_fields)) {
    return false;
  }

  auto trailing = BuildClassicCreate(TypeID::kUnit,
                                     ObjectGuid(0x1102030405060708ull));
  trailing.push_back(0u);
  if (ParseSuccessfully(trailing, UpdateObjectHandler{})) {
    return false;
  }

  return true;
}

}
