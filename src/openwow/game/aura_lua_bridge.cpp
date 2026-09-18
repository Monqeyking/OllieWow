
#include "openwow/game/aura_lua_bridge.h"

#include "openwow/data/formats/dbc/dbc_loader.h"
#include "openwow/game/aura_descriptor_sync.h"
#include "openwow/game/aura_tracker.h"
#include "openwow/game/update_fields.h"
#include "openwow/game/interaction_sender.h"
#include "openwow/game/script_event_helpers.h"
#include "openwow/game/spell_query_bridge.h"
#include "openwow/game/world_session.h"
#include "openwow/runtime/time/game_clock.h"
#include "openwow/ui/game/script_event_dispatch.h"

#include <algorithm>
#include <cstdint>

namespace openwow::game {

AuraLuaBridge& AuraLuaBridge::Get() {
  static AuraLuaBridge instance;
  return instance;
}

static AuraQueryResult BuildResult(const WorldSession& session,
                                   const AuraData& aura,
                                   const ObjectGuid& unit_guid) {
  AuraQueryResult r;
  r.spellId = aura.spell_id;
  r.slot = aura.slot;

  r.name = SpellQueryBridge::Get().GetSpellName(aura.spell_id);
  if (r.name.empty()) {
    r.name = "Spell #" + std::to_string(aura.spell_id);
  }
  r.rank = SpellQueryBridge::Get().GetSpellRank(aura.spell_id);

  if (const auto* dbc = session.GetDbcLoader(); dbc != nullptr) {
    const auto* spell = dbc->spell().LookupEntry(aura.spell_id);
    const auto* icon = spell != nullptr
                           ? dbc->spell_icon().LookupEntry(spell->spell_icon_id)
                           : nullptr;
    if (icon != nullptr && !icon->icon_path.empty()) {
      r.icon = std::string(icon->icon_path);
    }
  }

  r.count = aura.stacks;

  auto dispel = SpellQueryBridge::Get().GetSpellDispelType(aura.spell_id);
  r.debuffType = SpellQueryBridge::DispelTypeName(dispel);

  r.duration = static_cast<float>(aura.duration) / 1000.0f;

  if (aura.duration > 0 && aura.expiration > 0) {
    r.expirationTime = static_cast<double>(aura.expiration) / 1000.0;
    const auto remaining_ms = static_cast<std::int32_t>(
        aura.expiration - core::GameClock::GetTickCount32());
    r.remainingTime =
        std::max(0.0f, static_cast<float>(remaining_ms) / 1000.0f);
  }

  r.casterGuid = aura.caster_guid;
  if (aura.is_mine) {
    r.caster = "player";
  } else if (!aura.caster_guid.IsEmpty()) {
    r.caster = ui::game::UnitTokenRegistry::Get().TokenForAuraCasterGuid(
        aura.caster_guid.GetRawValue(), &session);
  } else {
    r.caster = "";
  }

  r.canStealOrPurge = ScriptAuraCanStealOrPurge(
      session, aura.spell_id, aura.effect_mask,
      aura.IsHelpfulByOriginalFlags(), unit_guid);

  if (const auto query = SpellQueryBridge::Get().Query(aura.spell_id);
      query.has_value()) {
    r.shouldConsolidate = (query->attributesEx6 & 0x00000100u) != 0u;
  }

  return r;
}

static bool MatchesNameRank(const AuraQueryResult& result,
                            const std::string& name,
                            const std::string& rank) {
  if (name != result.name) {
    return false;
  }

  return rank.empty() || rank == result.rank;
}

static bool MatchesFilter(const AuraData& aura, const std::string& filter) {
  if (filter.empty()) return true;

  const std::uint8_t flags = ParseAuraFilterFlags(filter, 1);
  const bool want_helpful = (flags & 0x03U) == 0x01U;
  const bool want_harmful = (flags & 0x03U) == 0x02U;
  const bool want_player = (flags & 0x04U) != 0U;
  const bool want_raid = (flags & 0x08U) != 0U;
  const bool want_cancelable = (flags & 0x10U) != 0U;
  const bool want_not_cancelable = (flags & 0x20U) != 0U;

  if (want_helpful && !aura.IsHelpfulByOriginalFlags()) return false;
  if (want_harmful && aura.IsHelpfulByOriginalFlags()) return false;

  if (want_player && !aura.is_mine) return false;

  if (want_cancelable && !aura.IsCancelableByOriginalFlags()) return false;
  if (want_not_cancelable && aura.IsCancelableByOriginalFlags()) return false;

  if (want_raid && aura.duration < 1800000) return false;

  return true;
}

static bool IsVisibleToScriptAuraQueries(const WorldSession& session,
                                         const AuraData& aura) {
  const auto caster_guid =
      aura.caster_guid.IsEmpty() ? nullptr : &aura.caster_guid;
  return SpellPassesScriptVisibilityFilter(
      session, aura.spell_id, caster_guid, false);
}

// 1.12 stuurt de aura-lijst in het create-blok, en daar dispatcht de
// descriptor-registry geen section-callbacks (descriptor_callback_registry.cpp:373).
// Zonder deze lazy sync blijft de tracker leeg tot de eerste aura-wijziging en
// toont de buffbalk niets -- precies wat er gebeurde met buffs die al op het
// character stonden bij het inloggen.
static void EnsureDescriptorAuras(const WorldSession& session,
                                  const ObjectGuid& unit_guid) {
  if (unit_guid.IsEmpty()) {
    return;
  }

  const auto* object = session.objects().GetUnit(unit_guid);
  if (object == nullptr) {
    return;
  }

  // Vergelijk het descriptor-blok met wat de tracker heeft. Alleen "is de tracker
  // leeg" is niet genoeg: bij een buff die tijdens de sessie bijkomt (bv. een
  // racial) bleef de tracker gevuld maar onvolledig, waardoor de nieuwe aura
  // nooit verscheen.
  std::uint32_t descriptor_count = 0;
  for (std::uint32_t slot = 0; slot < 48u; ++slot) {
    if (object->GetUInt32(UNIT_FIELD_AURA + slot) != 0u) {
      ++descriptor_count;
    }
  }

  std::uint32_t tracker_count = 0;
  AuraTracker::Get().ForEachAura(
      unit_guid, [&tracker_count](std::uint8_t, const AuraData&) {
        ++tracker_count;
      });
  if (descriptor_count == tracker_count) {
    return;
  }

  SyncAurasFromUnitDescriptor(session, unit_guid);

  // Zelfde reden als in world_session_object.cpp: de 1.12-BuffFrame ververst
  // alleen op PLAYER_AURAS_CHANGED. Via de queue, zodat een GetPlayerBuff-
  // aanroep vanuit BuffButton_Update niet re-entrant wordt.
  if (unit_guid == session.objects().GetLocalPlayerGuid()) {
    ui::game::ScriptEventDispatch::Get().QueueGlobalEvent(
        "PLAYER_AURAS_CHANGED");
  }
}

std::optional<AuraQueryResult> AuraLuaBridge::GetUnitBuff(
    const WorldSession& session, const ObjectGuid& unitGuid,
    std::uint32_t index) const {
  return GetUnitAura(session, unitGuid, index, "HELPFUL");
}

std::optional<AuraQueryResult> AuraLuaBridge::GetUnitDebuff(
    const WorldSession& session, const ObjectGuid& unitGuid,
    std::uint32_t index) const {
  return GetUnitAura(session, unitGuid, index, "HARMFUL");
}

std::optional<AuraQueryResult> AuraLuaBridge::GetUnitAura(
    const WorldSession& session, const ObjectGuid& unitGuid,
    std::uint32_t index,
    const std::string& filter) const {
  if (index == 0) return std::nullopt;

  EnsureDescriptorAuras(session, unitGuid);

  std::vector<const AuraData*> matching;
  AuraTracker::Get().ForEachAura(
      unitGuid,
      [&](std::uint8_t , const AuraData& aura) {
        if (IsVisibleToScriptAuraQueries(session, aura) &&
            MatchesFilter(aura, filter)) {
          matching.push_back(&aura);
        }
      });

  if (index > matching.size()) return std::nullopt;
  return BuildResult(session, *matching[index - 1], unitGuid);
}

std::optional<AuraQueryResult> AuraLuaBridge::FindUnitAura(
    const WorldSession& session, const ObjectGuid& unitGuid,
    const std::string& name,
    const std::string& rank, const std::string& filter) const {
  EnsureDescriptorAuras(session, unitGuid);

  std::optional<AuraQueryResult> result;
  AuraTracker::Get().ForEachAura(
      unitGuid,
      [&](std::uint8_t , const AuraData& aura) {
        if (result.has_value() ||
            !IsVisibleToScriptAuraQueries(session, aura) ||
            !MatchesFilter(aura, filter)) {
          return;
        }

        auto candidate =
            BuildResult(session, aura, unitGuid);
        if (MatchesNameRank(candidate, name, rank)) {
          result = std::move(candidate);
        }
      });

  return result;
}

std::optional<AuraQueryResult> AuraLuaBridge::UnitBuff(
    const WorldSession& session, const ObjectGuid& unitGuid,
    std::uint32_t index) const {
  return GetUnitBuff(session, unitGuid, index);
}

std::optional<AuraQueryResult> AuraLuaBridge::UnitDebuff(
    const WorldSession& session, const ObjectGuid& unitGuid,
    std::uint32_t index) const {
  return GetUnitDebuff(session, unitGuid, index);
}

void AuraLuaBridge::CancelUnitBuff(const WorldSession& session,
                                   const ObjectGuid& unitGuid,
                                   std::uint32_t index) {
  if (index == 0) return;

  std::uint8_t target_slot = 0;
  bool found = false;
  std::uint32_t buffCount = 0;
  AuraTracker::Get().ForEachAura(
      unitGuid,
      [&](std::uint8_t slot, const AuraData& aura) {
        if (aura.IsHelpfulByOriginalFlags() &&
            IsVisibleToScriptAuraQueries(session, aura)) {
          ++buffCount;
          if (buffCount == index && !found) {
            target_slot = slot;
            found = true;
          }
        }
      });

  if (found) {

    AuraTracker::Get().RemoveAura(unitGuid, target_slot);
  }
}

std::optional<AuraQueryResult> AuraLuaBridge::GetPlayerBuffByPosition(
    WorldSession& session, std::uint32_t position,
    const std::string& filter) const {
  const ObjectGuid player = session.objects().GetLocalPlayerGuid();
  if (player.IsEmpty()) return std::nullopt;

  EnsureDescriptorAuras(session, player);

  std::vector<const AuraData*> matching;
  AuraTracker::Get().ForEachAura(
      player, [&](std::uint8_t, const AuraData& aura) {
        // Alleen het 1.12 buffFilter (HELPFUL/HARMFUL/CANCELABLE/...), bewust
        // niet de 3.3.5-zichtbaarheidsregel: de 1.12-buffbalk toont elke aura
        // die in de slot staat.
        if (MatchesFilter(aura, filter)) {
          matching.push_back(&aura);
        }
      });

  if (position >= matching.size()) return std::nullopt;
  return BuildResult(session, *matching[position], player);
}

std::optional<AuraQueryResult> AuraLuaBridge::GetPlayerAuraByTrackerSlot(
    WorldSession& session, std::uint32_t slot) const {
  if (slot > 0xFFu) return std::nullopt;
  const ObjectGuid player = session.objects().GetLocalPlayerGuid();
  if (player.IsEmpty()) return std::nullopt;

  EnsureDescriptorAuras(session, player);

  const auto* aura =
      AuraTracker::Get().GetAura(player, static_cast<std::uint8_t>(slot));
  if (aura == nullptr) return std::nullopt;
  return BuildResult(session, *aura, player);
}

void AuraLuaBridge::CancelPlayerBuff(WorldSession& session,
                                     std::uint32_t slot) const {
  if (slot > 0xFFu) return;
  const ObjectGuid player = session.objects().GetLocalPlayerGuid();
  if (player.IsEmpty()) return;

  const auto* aura =
      AuraTracker::Get().GetAura(player, static_cast<std::uint8_t>(slot));
  if (aura == nullptr || !aura->IsHelpfulByOriginalFlags()) {
    return;
  }

  session.interaction().SendCancelAura(aura->spell_id);
  AuraTracker::Get().RemoveAura(player, static_cast<std::uint8_t>(slot));
}

WeaponEnchantResult AuraLuaBridge::GetWeaponEnchantInfo() const {
  return {has_mh_enchant_, mh_expiration_, mh_charges_,
          has_oh_enchant_, oh_expiration_, oh_charges_};
}

void AuraLuaBridge::SetMainHandEnchant(float expiration, int charges) {
  has_mh_enchant_ = true;
  mh_expiration_ = expiration;
  mh_charges_ = charges;
}

void AuraLuaBridge::SetOffHandEnchant(float expiration, int charges) {
  has_oh_enchant_ = true;
  oh_expiration_ = expiration;
  oh_charges_ = charges;
}

void AuraLuaBridge::ClearWeaponEnchants() {
  has_mh_enchant_ = false;
  mh_expiration_ = 0.0f;
  mh_charges_ = 0;
  has_oh_enchant_ = false;
  oh_expiration_ = 0.0f;
  oh_charges_ = 0;
}

}
