
#include "openwow/game/interaction_predicate.h"

#include "openwow/data/formats/dbc/dbc_enums.h"
#include "openwow/data/formats/dbc/dbc_loader.h"
#include "openwow/game/interaction_range.h"
#include "openwow/game/object_types.h"
#include "openwow/game/objects/cgplayer.h"
#include "openwow/game/objects/cgunit.h"

#include <cstdint>

namespace openwow::game {

namespace {

constexpr double kNonUnitInteractionRangeSquared = 25.0;

[[nodiscard]] bool ShapeshiftFormBlocksInteraction(const CGPlayer_C& player) {
  const auto form_id = player.State().SuppressesCurrentFormSpellQueries()
                           ? std::uint8_t{0}
                           : player.Animation().GetShapeshiftForm();
  if (form_id == 0u) {
    return false;
  }

  const auto* const dbc = player.dbc_loader();
  const auto* const form =
      dbc != nullptr ? dbc->spell_shapeshift_form().LookupEntry(form_id)
                     : nullptr;
  if (form == nullptr) {
    return false;
  }

  // Deliberately not keyed on ALLOW_ACTIVITY (0x1): this predicate also gates
  // the mouseover loot cursor and auto-loot (game_loop.cpp:1007-1023), and
  // Classic lets a player loot while shapeshifted. The form rules live where
  // they belong instead: talking to NPCs in
  // npc_interaction_controller.cpp (ALLOW_ACTIVITY | ALLOW_NPC_INTERACT, which
  // Source\src\game\Objects\Unit.cpp:6571-6580 mirrors for item use and
  // interaction) and game object use in cggameobject.cpp (0x8).
  // 0x100 is absent from the 14-field Classic record (see dbc_enums.h), so this
  // answer stays false for every Classic form.
  return (form->flags & data::dbc::kShapeshiftFormFlagBlocksAutoCancel) != 0u;
}

}

bool CanInteractWithTarget(const CGPlayer_C& active_player,
                           const CGObject_C& target) {

  const bool ignore_range =
      active_player.AutoInteractSuppressesInteractionRange();

  if (ShapeshiftFormBlocksInteraction(active_player)) {
    return false;
  }

  if (!ignore_range) {

    double budget_squared = kNonUnitInteractionRangeSquared;
    if (target.IsUnit()) {
      const auto& unit = static_cast<const CGUnit_C&>(target);
      budget_squared =
          static_cast<double>(interaction_range::ComputeUnitInteractionRangeSquared(
              active_player.State().GetCombatReach(),
              unit.State().GetCombatReach()));
    }

    if (active_player.GetSquaredDistanceToPosition(target.GetPosition()) >
        budget_squared) {
      return false;
    }
  }

  if (active_player.Animation().StandSelectionInteractionTargetGuid() != 0u) {
    return false;
  }

  if (active_player.State().IsDead()) {
    return false;
  }

  if ((active_player.GetMovementInfo().flags & kMoveFlagFalling) != 0u) {
    return false;
  }

  return !active_player.State().IsStunned() &&
         active_player.Movement().CanControlCharacter();
}

}
