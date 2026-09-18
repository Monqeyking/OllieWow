
#include "openwow/game/player_unit_field_event_callbacks.h"

#include "openwow/game/descriptor_callback_registry.h"
#include "openwow/game/objects/cgobject.h"
#include "openwow/game/script_event_helpers.h"
#include "openwow/game/update_fields.h"
#include "openwow/ui/game/script_event_dispatch.h"

#include <cstdint>
#include <cstring>

namespace openwow::game {

namespace {

std::uint32_t LowGuid(const ObjectGuid guid) {
    return static_cast<std::uint32_t>(guid.GetRawValue() & 0xFFFFFFFFull);
}

std::uint32_t HighGuidPart(const ObjectGuid guid) {
    return static_cast<std::uint32_t>(guid.GetRawValue() >> 32);
}

DescriptorCallbackBinding InventoryCallbackBinding() {
    static int key;
    return {reinterpret_cast<std::uintptr_t>(&key), 0};
}

DescriptorCallbackBinding QuestLogCallbackBinding() {
    static int key;
    return {reinterpret_cast<std::uintptr_t>(&key), 0};
}

}

// De per-unit-veld events komen uit MapChangedFieldsToEvents
// (update_field_event_mapper.cpp), dat op elke object-update draait en de
// benoemde 1.12-velden uit update_fields.h gebruikt.
//
// Hier stond een tweede pad: een tabel met 142 (index, eventnaam)-paren die per
// item een descriptor-callback registreerde. Die tabel was de 3.3.5-layout
// (inclusief UNIT_RUNIC_POWER, dat 1.12 niet heeft) terwijl het event-id en de
// byte-offset de 1.12-indeling volgden, waardoor elk item op het verkeerde veld
// hing: index 22 (UNIT_ENERGY) op UNIT_FIELD_HEALTH, index 48 (UNIT_LEVEL)
// midden in het aura-blok. Het leverde dus spook-events op bovenop de correcte
// events van de mapper. Gemeten op 2026-09-15: UNIT_HEALTH, UNIT_MAXHEALTH,
// UNIT_MANA en UNIT_TARGET vuren uit de mapper, dus dit tweede pad is niet
// nodig. Eén bron van waarheid, zoals de Benilla-referentie
// (benilla-app/src/ui_unit.rs, fire_transitions).

int PlayerInventoryChangedCallback(std::uint32_t guid_low,
                                   std::uint32_t guid_high) {
    std::uint64_t guid = static_cast<std::uint64_t>(guid_high) << 32
                       | static_cast<std::uint64_t>(guid_low);

    ScriptEvents_FireUnitEvent(guid, kEventInventoryChanged);
    return 1;
}

int PlayerQuestLogChangedCallback(std::uint32_t guid_low,
                                  std::uint32_t guid_high) {
    std::uint64_t guid = static_cast<std::uint64_t>(guid_high) << 32
                       | static_cast<std::uint64_t>(guid_low);

    ScriptEvents_FireUnitEvent(guid, kEventQuestLogChanged);
    return 1;
}

void Player_RegisterUnitFieldEventCallbacks(void* player_obj) {
    auto* object = static_cast<CGObject_C*>(player_obj);
    if (object == nullptr || !object->IsUnit()) {
        return;
    }

    Player_UnregisterUnitFieldEventCallbacks(player_obj);

    auto& registry = DescriptorCallbackRegistry::Get();
    const ObjectGuid guid = object->GetGuid();

    if (!object->IsPlayer() || !object->IsActivePlayer()) {
        return;
    }

    (void)registry.RegisterObjectSectionCallback(
        guid, TypeID::kPlayer, kPlayerInventoryOffset, kPlayerInventorySize,
        [](const DescriptorFieldChangeView& view) {
            PlayerInventoryChangedCallback(LowGuid(view.guid), HighGuidPart(view.guid));
        },
        InventoryCallbackBinding());

    for (std::uint32_t slot = 0; slot < kPlayerQuestLogSlotCount; ++slot) {
        const auto offset = static_cast<std::uint16_t>(
            kPlayerQuestLogOffset + slot * kPlayerQuestLogSlotSize);
        (void)registry.RegisterObjectSectionCallback(
            guid, TypeID::kPlayer, offset, kPlayerQuestLogSlotSize,
            [](const DescriptorFieldChangeView& view) {
                PlayerQuestLogChangedCallback(LowGuid(view.guid), HighGuidPart(view.guid));
            },
            QuestLogCallbackBinding());
    }
}

void Player_UnregisterUnitFieldEventCallbacks(void* player_obj) {
    auto* object = static_cast<CGObject_C*>(player_obj);
    if (object == nullptr || !object->IsUnit()) {
        return;
    }

    auto& registry = DescriptorCallbackRegistry::Get();
    const ObjectGuid guid = object->GetGuid();

    if (!object->IsPlayer()) {
        return;
    }

    registry.UnregisterObjectSectionCallback(
        guid, TypeID::kPlayer, kPlayerInventoryOffset,
        InventoryCallbackBinding());
    for (std::uint32_t slot = 0; slot < kPlayerQuestLogSlotCount; ++slot) {
        const auto offset = static_cast<std::uint16_t>(
            kPlayerQuestLogOffset + slot * kPlayerQuestLogSlotSize);
        registry.UnregisterObjectSectionCallback(
            guid, TypeID::kPlayer, offset, QuestLogCallbackBinding());
    }
}

}
